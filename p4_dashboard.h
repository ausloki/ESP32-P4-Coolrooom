#pragma once
// ============================================================================
// p4_dashboard.h — Serve the Coolroom web dashboard from firmware
//
// assets/dashboard.html is embedded gzip-compressed (p4_dashboard_html.h).
// Handler is PREPENDED so it wins over ESPHome's stock web UI at "/".
// Guest: main status + info. Operator: Login unlocks settings/admin.
//
// Rebuild after editing HTML or web credentials:
//   python3 scripts/embed_dashboard.py
// ============================================================================

#include <esp_log.h>
#include "esphome/components/web_server_base/web_server_base.h"

namespace p4_dash {

static const char *const TAG_DASH = "p4_dash";

class DashboardHandler : public esphome::web_server_idf::AsyncWebHandler {
 public:
  bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *request) const override {
    char buf[esphome::web_server_idf::AsyncWebServerRequest::URL_BUF_SIZE];
    esphome::StringRef url = request->url_to(buf);
    if (request->method() != HTTP_GET)
      return false;
    return url == "/" || url == "/assets/dashboard.html" || url == "/dashboard" ||
           url == "/dashboard.html";
  }

  void handleRequest(esphome::web_server_idf::AsyncWebServerRequest *request) override {
    // Serve the page at every alias — no redirect hop (faster first paint).
    auto *response =
        request->beginResponse(200, "text/html", P4_DASHBOARD_HTML_GZ, P4_DASHBOARD_HTML_GZ_LEN);
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  }

  bool isRequestHandlerTrivial() const override { return false; }
};

// AsyncWebServer::handlers_ is protected; accessor inserts at front so our
// "/" handler beats ESPHome's stock index (first match wins).
struct AsyncWebServerPrepend : public esphome::web_server_idf::AsyncWebServer {
  static void prepend(esphome::web_server_idf::AsyncWebServer *server,
                      esphome::web_server_idf::AsyncWebHandler *handler) {
    auto *self = static_cast<AsyncWebServerPrepend *>(server);
    self->handlers_.insert(self->handlers_.begin(), handler);
  }
};

inline void p4_dashboard_register() {
  auto *base = esphome::web_server_base::global_web_server_base;
  if (base == nullptr || base->get_server() == nullptr) {
    ESP_LOGW(TAG_DASH, "web_server not ready — dashboard not registered");
    return;
  }
  AsyncWebServerPrepend::prepend(base->get_server(), new DashboardHandler());
  ESP_LOGI(TAG_DASH, "Coolroom dashboard prepended at / (and /dashboard, /assets/dashboard.html)");
}

}  // namespace p4_dash
