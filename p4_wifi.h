#pragma once
// ============================================================================
// p4_wifi.h — ESP32-P4 Coolroom Controller: wireless status, scan, and a
// test-before-keep network switchover.
//
// Registers a custom HTTP handler on the same AsyncWebServer instance
// ESPHome's web_server uses (same pattern as p4_log_manager.h).
//
// Routes:
//   GET  /api/wifi          -> {"connected":bool,"ssid":"..","rssi":N,"channel":N,
//                               "bssid":"..","ip":"..","mac":"..","ap_active":bool,
//                               "ap_ssid":"..","switch_state":"..","message":".."}
//   POST /api/wifi/scan     -> {"ok":true}          (starts a scan, returns at once)
//   GET  /api/wifi/scan     -> {"state":"idle|scanning|done|failed",
//                               "networks":[{"ssid","rssi","channel","secure"},...]}
//   POST /api/wifi/connect?ssid=..&password=..  -> {"ok":true}
//
// Two things make this more than a thin wrapper around ESPHome's wifi API:
//
//  * Scanning without disturbing the connection. WiFiComponent::start_scanning()
//    forces the component's state machine into SCANNING, which on return picks
//    an AP and reconnects — i.e. it drops the live connection just to populate
//    a list. So the scan runs straight through esp_wifi instead, on a short-
//    lived task of its own: the blocking form is used because on this board
//    (P4 + esp_hosted co-processor) a WIFI_EVENT_SCAN_DONE handler registered
//    alongside ESPHome's never fired, leaving an event-driven version waiting
//    forever. Blocking needs a task rather than the main loop — a scan takes
//    seconds, and the display and control tick run there.
//
//  * Keeping the old network if the new one doesn't work. save_wifi_sta()
//    replaces the credential list and writes it to NVS immediately — with no
//    going back if the password was mistyped, leaving the fallback AP as the
//    only way in. Here the candidate is applied with set_sta(), which is RAM
//    only, and NVS is written after it has actually associated. So a failed
//    attempt is undone by putting the old credentials back, and a power cut
//    mid-attempt undoes it just as well: nothing was ever stored.
//
// All esp_wifi / ESPHome calls happen in poll(), which runs on the main loop.
// The HTTP handlers only ever set request flags and copy out prebuilt JSON,
// both under state_mutex — nothing here touches the radio from the httpd task.
// ============================================================================

#include <esp_err.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <vector>
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/hal.h"
#include "p4_logging.h"

namespace p4_wifi {

static const char *const TAG_P4WIFI = "p4_wifi";

/// How long a candidate network gets to associate before its credentials are
/// rolled back. Generous: a WPA2 association plus DHCP on a busy AP can take
/// well over ten seconds, and a needless revert is worse than a slow one.
static constexpr uint32_t SWITCH_TIMEOUT_MS = 45000;
/// Ignore "still connected" for this long after committing — the old link stays
/// up for a moment after the credentials change, and reading it as success
/// would keep an unreachable network.
static constexpr uint32_t SWITCH_SETTLE_MS = 8000;
/// How long the device may sit with no WiFi at all before the credentials it
/// booted with are assumed bad and the ones built into the firmware are put
/// back. Long enough to ride out a router reboot or a power cut that takes the
/// AP down with it, short enough that nobody has to fetch a laptop and a USB
/// cable. Only ever fires when disconnected, so a working device never sees it.
static constexpr uint32_t FACTORY_FALLBACK_MS = 300000;
/// A scan that never reports done (radio wedged, co-processor busy) must not
/// leave the UI spinning forever.
static constexpr uint32_t SCAN_TIMEOUT_MS = 25000;
/// Plenty for any real site; bounds the record buffer the scan task allocates.
static constexpr uint16_t SCAN_MAX_RESULTS = 40;

enum class ScanState : uint8_t { IDLE, RUNNING, DONE, FAILED };
enum class SwitchState : uint8_t { IDLE, TESTING, CONNECTED, REVERTED };

struct State {
  std::mutex mutex;

  // Scan
  bool scan_requested{false};
  volatile bool scan_task_running{false};
  ScanState scan_state{ScanState::IDLE};
  uint32_t scan_started_ms{0};
  std::string scan_json{"{\"state\":\"idle\",\"networks\":[]}"};

  // Switchover
  bool switch_requested{false};
  std::string pending_ssid;
  std::string pending_password;
  /// Held only for as long as the candidate is on trial — it is what gets
  /// written to NVS once the association succeeds, and discarded otherwise.
  std::string pending_switch_password;
  std::string prev_ssid;
  std::string prev_password;
  SwitchState switch_state{SwitchState::IDLE};
  uint32_t switch_started_ms{0};
  std::string switch_message;

  // Credentials compiled into the firmware, used as the last-resort fallback.
  std::string factory_ssid;
  std::string factory_password;
  uint32_t last_connected_ms{0};
  bool factory_fallback_applied{false};

  std::string status_json{"{\"connected\":false}"};
  bool registered{false};
};

inline State &state() {
  static State s;
  return s;
}

inline const char *scan_state_name(ScanState s) {
  switch (s) {
    case ScanState::RUNNING:
      return "scanning";
    case ScanState::DONE:
      return "done";
    case ScanState::FAILED:
      return "failed";
    default:
      return "idle";
  }
}

inline const char *switch_state_name(SwitchState s) {
  switch (s) {
    case SwitchState::TESTING:
      return "testing";
    case SwitchState::CONNECTED:
      return "connected";
    case SwitchState::REVERTED:
      return "reverted";
    default:
      return "idle";
  }
}

/// Minimal JSON string escaping — SSIDs are arbitrary bytes and regularly
/// contain quotes, backslashes and (on misconfigured APs) control characters.
inline std::string json_escape(const char *src, size_t len) {
  std::string out;
  for (size_t i = 0; i < len && src[i] != '\0'; i++) {
    unsigned char c = static_cast<unsigned char>(src[i]);
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out;
}

inline std::string json_escape(const std::string &s) { return json_escape(s.c_str(), s.size()); }

// ─── Scan (own task; blocking esp_wifi call) ────────────────────────────────

inline void scan_task_(void *param) {
  auto &s = state();
  wifi_scan_config_t cfg{};
  cfg.ssid = nullptr;
  cfg.bssid = nullptr;
  cfg.channel = 0;
  cfg.show_hidden = false;
  cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
  cfg.scan_time.active.min = 100;
  cfg.scan_time.active.max = 300;

  std::string json = "{\"state\":\"failed\",\"networks\":[]}";
  bool ok = false;

  esp_err_t err = esp_wifi_scan_start(&cfg, true);
  if (err != ESP_OK) {
    ESP_LOGW(TAG_P4WIFI, "esp_wifi_scan_start failed: %s", esp_err_to_name(err));
  } else {
    uint16_t number = 0;
    esp_wifi_scan_get_ap_num(&number);
    if (number > SCAN_MAX_RESULTS)
      number = SCAN_MAX_RESULTS;

    json = "{\"state\":\"done\",\"networks\":[";
    size_t count = 0;
    if (number > 0) {
      // One bulk read, not one record at a time: per ESPHome's own note, the
      // single-record call fails on P4 with the hosted WiFi co-processor.
      auto *records = new (std::nothrow) wifi_ap_record_t[number];
      if (records != nullptr) {
        uint16_t got = number;
        if (esp_wifi_scan_get_ap_records(&got, records) == ESP_OK) {
          for (uint16_t i = 0; i < got; i++) {
            const char *ssid = reinterpret_cast<const char *>(records[i].ssid);
            if (ssid[0] == '\0')
              continue;  // hidden network — nothing an operator could pick out of a list
            if (count > 0)
              json += ",";
            char tail[96];
            snprintf(tail, sizeof(tail), "\",\"rssi\":%d,\"channel\":%u,\"secure\":%s}",
                     (int) records[i].rssi, (unsigned) records[i].primary,
                     records[i].authmode != WIFI_AUTH_OPEN ? "true" : "false");
            json += "{\"ssid\":\"" + json_escape(ssid, sizeof(records[i].ssid)) + tail;
            count++;
          }
        } else {
          esp_wifi_clear_ap_list();
        }
        delete[] records;
      }
    }
    json += "]}";
    ESP_LOGI(TAG_P4WIFI, "WiFi scan complete — %u network(s)", (unsigned) count);
    ok = true;
  }

  {
    std::lock_guard<std::mutex> lock(s.mutex);
    s.scan_json = json;
  }
  s.scan_state = ok ? ScanState::DONE : ScanState::FAILED;
  s.scan_task_running = false;
  vTaskDelete(nullptr);
}

inline void start_scan_(uint32_t now) {
  auto &s = state();
  s.scan_state = ScanState::RUNNING;
  s.scan_started_ms = now;
  s.scan_task_running = true;
  // Records are heap-allocated, so the stack only carries the JSON building.
  if (xTaskCreate(&scan_task_, "p4_wifi_scan", 4096, nullptr, 3, nullptr) != pdPASS) {
    ESP_LOGW(TAG_P4WIFI, "Could not start scan task");
    s.scan_task_running = false;
    s.scan_state = ScanState::FAILED;
    std::lock_guard<std::mutex> lock(s.mutex);
    s.scan_json = "{\"state\":\"failed\",\"networks\":[]}";
    return;
  }
  ESP_LOGI(TAG_P4WIFI, "WiFi scan started");
}

// ─── Called from the main loop (interval) ───────────────────────────────────

/// `now` is the caller's tick timestamp rather than a fresh millis() read:
/// taking a new reading here puts the start a millisecond or two *after* the
/// tick that evaluates the deadline in the same pass, and the unsigned
/// subtraction that computes elapsed time then wraps to roughly 49 days —
/// long enough to trip every deadline in this file on the spot.
inline void commit_switch_(uint32_t now, const std::string &ssid, const std::string &password) {
  auto &s = state();
  auto *wc = esphome::wifi::global_wifi_component;
  if (wc == nullptr)
    return;

  // Captured before the candidate replaces the credential list — this is the
  // only moment the outgoing password is still readable.
  auto prev = wc->get_sta();
  s.prev_ssid = std::string(prev.get_ssid().c_str(), prev.get_ssid().size());
  s.prev_password = std::string(prev.get_password().c_str(), prev.get_password().size());

  ESP_LOGI(TAG_P4WIFI, "Trying WiFi network '%s' (current: '%s')", ssid.c_str(), s.prev_ssid.c_str());
  p4_sd_log_event("WIFI_SWITCH_TRY", ssid.c_str());

  esphome::wifi::WiFiAP candidate{};
  candidate.set_ssid(ssid);
  candidate.set_password(password);
  wc->set_sta(candidate);
  s.pending_switch_password = password;
  // set_sta() swaps the credentials but leaves a healthy link up, so nothing
  // would actually move until it happened to drop. Dropping it here is what
  // makes this a test of the new network.
  esp_wifi_disconnect();

  s.switch_state = SwitchState::TESTING;
  s.switch_started_ms = now;
  std::lock_guard<std::mutex> lock(s.mutex);
  s.switch_message = "Connecting to " + ssid + "…";
}

inline void revert_switch_() {
  auto &s = state();
  auto *wc = esphome::wifi::global_wifi_component;
  ESP_LOGW(TAG_P4WIFI, "New network did not connect — reverting to '%s'", s.prev_ssid.c_str());
  p4_sd_log_event("WIFI_SWITCH_REVERT", s.prev_ssid.c_str());
  if (wc != nullptr && !s.prev_ssid.empty()) {
    esphome::wifi::WiFiAP previous{};
    previous.set_ssid(s.prev_ssid);
    previous.set_password(s.prev_password);
    wc->set_sta(previous);
    esp_wifi_disconnect();
  }
  s.pending_switch_password.clear();
  s.switch_state = SwitchState::REVERTED;
  std::lock_guard<std::mutex> lock(s.mutex);
  s.switch_message = s.prev_ssid.empty()
                         ? "Could not connect, and no previous network was stored"
                         : ("Could not connect — back on " + s.prev_ssid);
}

/// SSID this device is actually associated to right now, empty if none.
///
/// The authority on whether a switchover worked. WiFiComponent::is_connected()
/// alone is not: its flag is updated from queued events, so for a moment after
/// the link is deliberately dropped it still reads true — which was enough to
/// declare an unreachable network a success and write its credentials to NVS,
/// where they replace the configured network on every subsequent boot.
inline std::string associated_ssid_() {
  wifi_ap_record_t ap{};
  if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK)
    return "";
  const char *ssid = reinterpret_cast<const char *>(ap.ssid);
  return std::string(ssid, strnlen(ssid, sizeof(ap.ssid)));
}

/// Last line of defence for the credentials the device boots with.
///
/// A saved network doesn't sit alongside the one in the firmware, it replaces
/// it (see WiFiComponent::start()), so a device that saved a network which
/// later stops working — renamed, retired, password rotated — has no way back
/// on its own, and the fallback AP only helps someone standing next to it.
/// After a long stretch with no connection at all, the firmware's own
/// credentials are restored.
inline void maybe_restore_factory_network_() {
  auto &s = state();
  auto *wc = esphome::wifi::global_wifi_component;
  if (wc == nullptr || s.factory_ssid.empty() || s.factory_fallback_applied)
    return;
  if (s.switch_state == SwitchState::TESTING)
    return;  // a switchover is being evaluated on its own, much shorter, clock
  if (esphome::millis() - s.last_connected_ms < FACTORY_FALLBACK_MS)
    return;

  auto current = wc->get_sta();
  std::string current_ssid(current.get_ssid().c_str(), current.get_ssid().size());
  if (current_ssid == s.factory_ssid)
    return;  // already on the firmware's network — nothing left to fall back to

  ESP_LOGW(TAG_P4WIFI, "No WiFi for %u min on '%s' — restoring configured network '%s'",
           (unsigned) (FACTORY_FALLBACK_MS / 60000), current_ssid.c_str(), s.factory_ssid.c_str());
  p4_sd_log_event("WIFI_FACTORY_FALLBACK", s.factory_ssid.c_str());
  wc->save_wifi_sta(s.factory_ssid, s.factory_password);
  esp_wifi_disconnect();
  s.factory_fallback_applied = true;
  std::lock_guard<std::mutex> lock(s.mutex);
  s.switch_message = "No connection for " + std::to_string(FACTORY_FALLBACK_MS / 60000) +
                     " minutes — restored the network configured in firmware";
}

inline void refresh_status_() {
  auto &s = state();
  auto *wc = esphome::wifi::global_wifi_component;
  bool connected = wc != nullptr && wc->is_connected();

  std::string ssid;
  int rssi = 0;
  unsigned channel = 0;
  char bssid[18] = "";
  if (connected) {
    wifi_ap_record_t ap{};
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
      ssid = json_escape(reinterpret_cast<const char *>(ap.ssid), sizeof(ap.ssid));
      rssi = ap.rssi;
      channel = ap.primary;
      snprintf(bssid, sizeof(bssid), "%02X:%02X:%02X:%02X:%02X:%02X", ap.bssid[0], ap.bssid[1], ap.bssid[2],
               ap.bssid[3], ap.bssid[4], ap.bssid[5]);
    }
  }

  char ip[16] = "";
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (netif != nullptr) {
    esp_netif_ip_info_t info{};
    if (esp_netif_get_ip_info(netif, &info) == ESP_OK)
      snprintf(ip, sizeof(ip), IPSTR, IP2STR(&info.ip));
  }

  char mac[18] = "";
  uint8_t raw[6]{};
  if (esp_wifi_get_mac(WIFI_IF_STA, raw) == ESP_OK)
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]);

  bool ap_active = false;
  std::string ap_ssid;
#ifdef USE_WIFI_AP
  if (wc != nullptr) {
    ap_active = wc->is_ap_active();
    auto ap = wc->get_ap();
    ap_ssid = json_escape(ap.get_ssid().c_str(), ap.get_ssid().size());
  }
#endif

  std::lock_guard<std::mutex> lock(s.mutex);
  s.status_json = std::string("{\"connected\":") + (connected ? "true" : "false") + ",\"ssid\":\"" + ssid +
                  "\",\"rssi\":" + std::to_string(rssi) + ",\"channel\":" + std::to_string(channel) +
                  ",\"bssid\":\"" + bssid + "\",\"ip\":\"" + ip + "\",\"mac\":\"" + mac + "\",\"ap_active\":" +
                  (ap_active ? "true" : "false") + ",\"ap_ssid\":\"" + ap_ssid + "\",\"switch_state\":\"" +
                  switch_state_name(s.switch_state) + "\",\"message\":\"" + json_escape(s.switch_message) + "\"}";
}

/// Drives every asynchronous piece of this module. Call once per second from
/// the main loop; everything else here only reads or writes cached state.
inline void poll() {
  auto &s = state();
  const uint32_t now = esphome::millis();

  bool want_scan = false;
  bool want_switch = false;
  std::string ssid, password;
  {
    std::lock_guard<std::mutex> lock(s.mutex);
    want_scan = s.scan_requested;
    s.scan_requested = false;
    want_switch = s.switch_requested;
    s.switch_requested = false;
    ssid = s.pending_ssid;
    password = s.pending_password;
    s.pending_password.clear();
  }

  if (want_scan && !s.scan_task_running)
    start_scan_(now);

  // The task publishes its own result; this only stops a wedged scan from
  // leaving the UI waiting on a state that will never change.
  if (s.scan_state == ScanState::RUNNING && !s.scan_task_running &&
      now - s.scan_started_ms > SCAN_TIMEOUT_MS) {
    ESP_LOGW(TAG_P4WIFI, "WiFi scan timed out");
    s.scan_state = ScanState::FAILED;
    std::lock_guard<std::mutex> lock(s.mutex);
    s.scan_json = "{\"state\":\"failed\",\"networks\":[]}";
  }

  if (want_switch && !ssid.empty() && s.switch_state != SwitchState::TESTING)
    commit_switch_(now, ssid, password);

  if (s.switch_state == SwitchState::TESTING) {
    const uint32_t elapsed = now - s.switch_started_ms;
    auto *wc = esphome::wifi::global_wifi_component;
    const bool on_candidate = associated_ssid_() == s.pending_ssid;
    if (elapsed >= SWITCH_SETTLE_MS && on_candidate && wc != nullptr && wc->is_connected()) {
      // Proven — only now does it become the network this device boots onto.
      wc->save_wifi_sta(s.pending_ssid, s.pending_switch_password);
      s.pending_switch_password.clear();
      ESP_LOGI(TAG_P4WIFI, "New WiFi network connected — credentials saved");
      p4_sd_log_event("WIFI_SWITCH_OK", s.pending_ssid.c_str());
      s.switch_state = SwitchState::CONNECTED;
      std::lock_guard<std::mutex> lock(s.mutex);
      s.switch_message = "Connected to " + s.pending_ssid;
    } else if (elapsed >= SWITCH_TIMEOUT_MS) {
      revert_switch_();
    }
  }

  {
    auto *wc = esphome::wifi::global_wifi_component;
    if (wc != nullptr && wc->is_connected() && !associated_ssid_().empty())
      s.last_connected_ms = now;
  }
  maybe_restore_factory_network_();

  refresh_status_();
}

// ─── Called from the HTTP task ──────────────────────────────────────────────

inline void request_scan() {
  auto &s = state();
  std::lock_guard<std::mutex> lock(s.mutex);
  s.scan_requested = true;
  if (!s.scan_task_running)
    s.scan_json = "{\"state\":\"scanning\",\"networks\":[]}";
}

inline void request_switch(const std::string &ssid, const std::string &password) {
  auto &s = state();
  std::lock_guard<std::mutex> lock(s.mutex);
  s.pending_ssid = ssid;
  s.pending_password = password;
  s.switch_requested = true;
  s.switch_state = SwitchState::IDLE;
  s.switch_message = "Applying…";
}

class WifiHandler : public esphome::web_server_idf::AsyncWebHandler {
 public:
  bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *request) const override {
    char buf[esphome::web_server_idf::AsyncWebServerRequest::URL_BUF_SIZE];
    esphome::StringRef url = request->url_to(buf);
    auto method = request->method();
    if (url == "/api/wifi" && method == HTTP_GET)
      return true;
    if (url == "/api/wifi/scan" && (method == HTTP_GET || method == HTTP_POST))
      return true;
    if (url == "/api/wifi/connect" && method == HTTP_POST)
      return true;
    return false;
  }

  void handleRequest(esphome::web_server_idf::AsyncWebServerRequest *request) override {
    char buf[esphome::web_server_idf::AsyncWebServerRequest::URL_BUF_SIZE];
    esphome::StringRef url = request->url_to(buf);
    auto &s = state();

    if (url == "/api/wifi") {
      std::lock_guard<std::mutex> lock(s.mutex);
      request->send(200, "application/json", s.status_json.c_str());
      return;
    }

    if (url == "/api/wifi/scan") {
      if (request->method() == HTTP_POST) {
        request_scan();
        request->send(200, "application/json", "{\"ok\":true,\"state\":\"scanning\"}");
      } else {
        std::string json;
        {
          std::lock_guard<std::mutex> lock(s.mutex);
          json = s.scan_json;
        }
        request->send(200, "application/json", json.c_str());
      }
      return;
    }

    if (url == "/api/wifi/connect") {
      std::string ssid = request->arg("ssid");
      std::string password = request->arg("password");
      if (ssid.empty()) {
        request->send(400, "application/json", "{\"ok\":false,\"error\":\"missing_ssid\"}");
        return;
      }
      request_switch(ssid, password);
      request->send(200, "application/json", "{\"ok\":true,\"state\":\"testing\"}");
    }
  }

  bool isRequestHandlerTrivial() const override { return false; }
};

/// Credentials from the firmware's own wifi: config, kept so a saved network
/// that stops working can be backed out of without a USB cable.
inline void set_factory_network(const char *ssid, const char *password) {
  auto &s = state();
  s.factory_ssid = ssid == nullptr ? "" : ssid;
  s.factory_password = password == nullptr ? "" : password;
  s.last_connected_ms = esphome::millis();  // start the no-connection clock at boot
}

inline void p4_wifi_register() {
  auto &s = state();
  if (s.registered)
    return;
  auto *base = esphome::web_server_base::global_web_server_base;
  if (base == nullptr) {
    ESP_LOGW(TAG_P4WIFI, "web_server not ready — wireless endpoints not registered");
    return;
  }
  base->add_handler(new WifiHandler());
  s.registered = true;
  ESP_LOGI(TAG_P4WIFI, "Wireless endpoints registered at /api/wifi");
}

}  // namespace p4_wifi
