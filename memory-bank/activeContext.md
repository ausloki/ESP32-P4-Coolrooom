# Active Context — Current Session State

**Date:** 2026-07-24  
**Session:** Security fix — leaked dashboard credential + RBAC model cleanup  
**Status:** BUILDABLE, PHASE 5 STILL ACTIVE, DEVICE REFLASH PENDING

## Current Focus

### What Was Confirmed This Session

- A project evaluation found `assets/dashboard.html` and `assets/dashboard_virtual_preview.html`
  hardcoded the real device password (`P@lli5ter`) as the "admin"/"superadmin" demo login,
  committed to git since Phase 12. That value matched the real `web_server_password`/
  `ota_password` in `secrets.yaml` — anyone viewing the dashboard's page source had the real
  device/OTA credential.
- The three-tier guest/admin/superadmin model was never backed by anything server-side —
  ESPHome's `web_server.auth` supports exactly one username/password pair, so the tier split and
  the `web_admin_users` role-mapping comment in `esp32-p4-coolroom.yaml` described a feature
  that was never implemented.

### Latest Completed Work

- Rotated `ota_password` and `web_server_password` in `secrets.yaml` (git-ignored, not
  committed) to new random values.
- Reworked `assets/dashboard.html`: login now verifies the entered credential against the live
  device instead of a hardcoded value; collapsed to the two tiers that actually exist (guest /
  operator); removed the fake "User Management" panel.
- Fixed the same leaked-password issue in `assets/dashboard_virtual_preview.html`.
- Rewrote `reference/RBAC_USER_GUIDE.md` and `reference/AUTHENTICATION_GUIDE.md` to match the
  real two-tier, UI-only-visibility model.
- Removed the misleading `web_admin_users` comment from `esp32-p4-coolroom.yaml`.
- Repo-wide grep confirmed no remaining references to the leaked password string.

### Build Status

```text
Compile: successful via ./tools/esphome_compile.sh
RAM:   19.5% (112,176 / 576,464 bytes)
Flash: 20.3% (1,491,976 / 7,340,032 bytes; about 1.49 MB of a 7 MB OTA slot)
Warning level: only generic ESP-IDF experimental-features warning remains
```

### Immediate Next Actions

1. **Reflash the physical device** (`esphome upload`) so the rotated OTA/web credentials take
   effect — this is the one item from this session that cannot be closed out from the repo
   alone. Until reflashed, the device still expects the old (now-public-in-history) password.
2. Consider whether the leaked password should also be scrubbed from git history
   (`git filter-repo`/BFG) — rotation matters more, but history scrubbing was flagged as an
   option.
3. Resume the pre-existing Phase 5 hardware validation items (see Outstanding Items below) —
   this session's work was a security fix, not Phase 5 feature progress.

### Outstanding Items

1. Phase 5 is still open in the firmware header and instructions.
2. Hardware validation is still required for the offline-safe and SD-card changes on this
   ESP32-P4 board (no-reboot-on-WiFi-loss, SD write behavior, ntfy reconnect behavior).
3. RTC identity (0x51 vs 0x68) still needs confirmation via I2C scan or chip marking inspection.
4. Device reflash for the rotated credentials (see Immediate Next Actions #1) is outstanding.
5. If real server-side dashboard authorization is ever wanted, it requires either ESPHome
   gaining multi-account/role support or a proxy in front of `web_server` — don't reintroduce
   fake role tiers in the dashboard in the meantime.

### Key Anchors For Resume

- Firmware status header: `esp32-p4-coolroom.yaml`
- Current recap history: `reference/session_recaps.md`
- Main resume handover: `HANDOVER_NOTES_2026-07-18.md`
- Dashboard access model: `reference/RBAC_USER_GUIDE.md`, `reference/AUTHENTICATION_GUIDE.md`
- Historical LVGL handover: `reference/HANDOVER_2026-07-18.md`

---

**Ready for:** Device reflash to apply rotated credentials, then Phase 5 hardware validation on
the ESP32-P4 target.
