# Dashboard Access Guide

## Scope

This guide describes the current access-control behavior implemented in `assets/dashboard.html`.

## Important: this is UI visibility control, not server-side RBAC

ESPHome's `web_server` component supports exactly one HTTP Basic Auth username/password pair —
there is no concept of multiple accounts or privilege tiers on the device itself. Because of that:

- The dashboard has exactly two states: **guest** (not logged in, read-only view) and
  **operator** (logged in with the device's real credential, full view + control).
- There is no "admin" vs "superadmin" distinction anywhere in this system. Any earlier
  documentation describing three tiers, or a `web_admin_users` role-mapping setting, described
  something that was never actually implemented server-side — it has been removed.
- Logging in checks the entered username/password against the live device
  (`GET /api/states` with an `Authorization: Basic` header) rather than a value stored in the
  page. There is nothing else to check it against, since the browser session has no separate
  backend of its own.
- Once elevated, the operator still only has whatever the device's single credential grants —
  i.e. everything. Section-level hiding in the UI (settings vs. administration) is a workflow
  convenience, not an access boundary. Anyone with the device credential (or anyone who can
  reach the unauthenticated read endpoints, if any are exposed) has full control regardless of
  what the dashboard chooses to show them.

## Access Levels

### Guest

Purpose: view-only monitoring. Default state on page load, no login required.

Visible:

- Main display metrics, alarm/status indicators, system health section.

Hidden (UI-only):

- Operational settings section.
- System administration section.

### Operator

Purpose: operational control and administration — the only elevated tier that exists.

Elevation method: click `🔐 Login` and enter the same `web_server_username` /
`web_server_password` configured in the device's `secrets.yaml`. The dashboard verifies this
against the live device before elevating.

Visible:

- All guest sections, plus operational settings and system administration sections.

Note: the backup/restore, log management, and Wi-Fi/hardware buttons in the administration
section are UI placeholders — see `esp32-p4-coolroom.yaml` / `p4_logging.h` for what's actually
implemented server-side (SD backup/restore exists via physical buttons on the device; there is
no dashboard-triggered backup/restore, log export, or WiFi config endpoint yet).

## Elevation Flow

1. Open the dashboard URL in a browser.
2. Dashboard loads in guest mode.
3. Click `🔐 Login` in the header.
4. Enter the device's real username/password.
5. On success (device returns `200` for the credential), the role badge updates to `Operator`
   and the settings/admin sections unhide. On failure (`401`), the login is rejected.
6. Click `🚪 Logout` to return to guest mode, or wait for the 2-minute inactivity timeout.

## Usage

Basic access (served directly by the device):

```text
http://<device-ip>/assets/dashboard.html
```

Hosted access (served from an external web server, pointed at the device via `?ha_host=`):

```text
https://<dashboard-host>/coolroom.html?ha_host=<device-ip>
```

## Troubleshooting

### Login rejected

- Confirm the username/password matches `web_server_username` / `web_server_password` in the
  device's `secrets.yaml` exactly.
- Confirm the dashboard can actually reach the device (check the network tab for the
  `/api/states` request) — an unreachable device and a wrong password both surface as a failed
  login, but only one of them is a credential problem.

### Operator cannot see settings/admin sections after login

- Confirm the role badge shows `Operator`.
- Reload the page and log in again.

## Related Files

- `assets/dashboard.html` — active dashboard implementation.
- `assets/dashboard_virtual_preview.html` — offline static UI preview (never talks to a real
  device; its login uses a preview-only placeholder credential, not a real device password).
- `HANDOVER_NOTES_2026-07-18.md` — implementation history.

## Status

- Last reviewed: 2026-07-24 — collapsed to the real two-tier (guest/operator) model and removed
  the hardcoded demo password that had been reused as the actual device credential.
