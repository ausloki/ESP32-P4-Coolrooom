# Role-Based Access Control (RBAC) User Guide

## Scope

This guide describes the current RBAC behavior implemented in `assets/dashboard.html`.

## Summary

The dashboard now uses a guest-first model.

1. On first load, the dashboard starts in guest mode.
2. No login is required for guest viewing.
3. The `🔐 Login` button is used only for elevation to `admin` or `superadmin`.
4. `🚪 Logout` always de-elevates back to guest mode.

## Access Levels

### Guest

Purpose: view-only monitoring.

Visible sections:

- Main display metrics.
- Alarm and status indicators.
- System health section.

Restricted sections:

- Operational settings.
- System administration.

Allowed actions:

- View current values and status.
- Observe alarms and health indicators.

Denied actions:

- Modify setpoint and operational controls.
- Use backup/restore administration controls.
- Use Wi-Fi/hardware administration controls.

### Admin

Purpose: day-to-day operational control.

Visible sections:

- All guest sections.
- Operational settings section.

Restricted sections:

- System administration section.

Allowed actions:

- Modify setpoint.
- Modify alarm thresholds.
- Modify defrost and hysteresis operational controls exposed in the dashboard.

Denied actions:

- Backup/restore administration actions.
- Log deletion/download administration actions.
- Wi-Fi/hardware administration actions.

Elevation method: click `🔐 Login` and authenticate as `admin`.

### Superadmin

Purpose: full dashboard administration.

Visible sections:

- All guest and admin sections.
- System administration section.

Allowed actions:

- All admin actions.
- Backup/restore actions.
- Log management actions.
- Wi-Fi/hardware administration actions.

Elevation method: click `🔐 Login` and authenticate as `superadmin`.

## Elevation Flow

1. Open dashboard URL in browser.
2. Dashboard loads in guest mode.
3. Click `🔐 Login` in the header.
4. Enter credentials for `admin` or `superadmin`.
5. Role badge updates and restricted sections unlock per permissions.
6. Click `🚪 Logout` to return to guest mode.

## Demo Credentials

- `admin / P@lli5ter`
- `superadmin / P@lli5ter`

Guest does not require credentials because guest mode is the default state.

## Permission Model

The dashboard enforces permissions client-side via role matrix checks.

- Visibility is role-controlled for settings and admin sections.
- Mutating controls perform role checks before action handlers execute.

Note: this is operational workflow control, not server-side cryptographic authorization.

## Usage

Basic access:

```text
http://<device-ip>/assets/dashboard.html
```

Hosted access:

```text
https://<dashboard-host>/coolroom.html
```

## Feature Matrix

| Feature | Guest | Admin | Superadmin |
| ------- | ----- | ----- | ---------- |
| View temperature/status/health | ✅ | ✅ | ✅ |
| Change setpoint and operational settings | ❌ | ✅ | ✅ |
| Backup settings | ❌ | ❌ | ✅ |
| Restore settings | ❌ | ❌ | ✅ |
| Delete/download logs | ❌ | ❌ | ✅ |
| Wi-Fi and hardware admin controls | ❌ | ❌ | ✅ |

## Troubleshooting

### Dashboard stays in guest after login attempt

- Verify username is `admin` or `superadmin`.
- Verify password matches configured credential.

### Admin cannot see settings section

- Confirm role badge shows `Administrator`.
- Reload page and elevate again.

### Superadmin cannot see admin section

- Confirm role badge shows `Super Administrator`.
- Reload page and elevate again.

### Buttons show permission warnings

- Expected in guest mode for mutating/admin actions.
- Elevate with login if action is required.

## Related Files

- `assets/dashboard.html` for active RBAC implementation.
- `assets/dashboard_virtual_preview.html` for local navigable RBAC preview.
- `HANDOVER_NOTES_2026-07-18.md` for implementation history.

## Status

- Phase: 12 RBAC model in use.
- Last reviewed: 2026-07-24.
