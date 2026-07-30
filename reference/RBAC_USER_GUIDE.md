# Dashboard Access Guide

## Scope

This guide describes the current access-control behavior implemented in `assets/dashboard.html`.

## Important: this is UI visibility control, not server-side RBAC

ESPHome cannot do “guest may read `/api/states`, operator-only may write” with a single
`web_server.auth` gate — Basic Auth would block guest live metrics too. This project therefore:

- Leaves **HTTP Basic Auth off** on the device web server / REST API (LAN trust).
- Serves the Coolroom dashboard as the default page (`/` → `/assets/dashboard.html`).
- Distinguishes **guest** (not logged in, read-only UI) vs **operator** (logged in, full UI)
  only in the dashboard.
- Verifies Login by comparing SHA-256(`username:password`) to a token baked into the
  firmware at build time from `secrets.yaml` (`scripts/embed_dashboard.py`). Changing the
  web password requires re-running that script and reflashing.

There is no "admin" vs "superadmin" tier. Anyone who can reach the LAN API can change
entities with raw HTTP; the Login button only unlocks the dashboard form controls.

## Access Levels

### Guest

Purpose: view-only monitoring. Default state on page load, **no login required**.

Visible:

- Main display metrics, alarm/status indicators, system health section.
- Settings/admin sections remain visible but controls are disabled; lock chips say
  “Login to edit” (tap to open the login modal).

### Operator

Purpose: operational control and administration — the only elevated tier that exists.

Elevation method: click `🔐 Login` (or a lock chip) and enter `web_server_username` /
`web_server_password` from `secrets.yaml`. The dashboard hashes the pair and compares it to
the build-time token — no browser Basic Auth prompt, and no dependency on `/api/states` 401.

Visible:

- All guest sections, plus interactive operational settings and system administration.

## Elevation Flow

1. Open `http://<device-ip>/` (or `/dashboard`).
2. Dashboard loads in guest mode and refreshes live data without credentials.
3. Click `🔐 Login` when you need settings/admin.
4. Enter the device username/password.
5. On success, the role badge updates to `Operator` and controls unlock. On failure, the
   login is rejected (wrong password or firmware built without a fresh embed after a
   secrets change).
6. Click `🚪 Logout` to return to guest mode, or wait for the 2-minute inactivity timeout.

## Usage

```text
http://<device-ip>/
http://<device-ip>/dashboard
http://<device-ip>/assets/dashboard.html
```

Optional API host override when the HTML is hosted elsewhere:

```text
https://<dashboard-host>/coolroom.html?api_host=<device-ip>
```

## Troubleshooting

### Login rejected

- Confirm the username/password matches `web_server_username` / `web_server_password` in
  `secrets.yaml` exactly (password was rotated historically — old leaked values will fail).
- After changing secrets, run `python3 scripts/embed_dashboard.py` then compile/flash.
- Clear any old browser-cached Basic Auth for the device IP (no longer used).

### Main page asks for a password / metrics empty

- You should not get a browser Basic Auth dialog on `/` or `/dashboard`. If you do, an old
  firmware with `web_server.auth` is still running — reflash.
- Guest metrics need reachable `GET /api/states` (same origin as the page). Check the
  browser network tab.

### Operator cannot edit settings after login

- Confirm the role badge shows `Operator`.
- Reload the page and log in again.

## Related Files

- `assets/dashboard.html` — active dashboard implementation.
- `scripts/embed_dashboard.py` — gzip embed + auth-token injection.
- `p4_dashboard.h` / `p4_dashboard_html.h` — firmware handlers + embedded page.
- `secrets.yaml` — `web_server_username` / `web_server_password`.
- `reference/AUTHENTICATION_GUIDE.md` — broader auth notes.
