# Dashboard Session & Inactivity Guide

## Scope

This guide covers session/inactivity mechanics in `assets/dashboard.html`. For the login flow,
access levels, and what "operator" actually means, see `reference/RBAC_USER_GUIDE.md` first —
this document does not restate that model.

> This file previously described a three-tier `guest`/`admin`/`superadmin` system with
> hardcoded demo passwords, a "SuperAdmin" password-change feature, and localStorage session
> persistence. None of that reflected the actual code even before the 2026-07-24 dashboard
> rewrite — the account tiers were never backed by anything server-side, the password-change
> feature only wrote to `localStorage` (it never touched the device), and session state was
> never actually restored from `localStorage` on reload despite what the old doc claimed. That
> content has been removed rather than fixed in place, since ESPHome's `web_server` genuinely
> has no multi-account/role support to build it on.

## Current Behavior

- The dashboard has two states: **guest** (default, view-only metrics) and **operator**
  (logged in with the device credential from `secrets.yaml`). See `RBAC_USER_GUIDE.md`.
- Session state (`currentRole`) lives only in page memory — it is **not** persisted to
  `localStorage` or any cookie. Reloading the page always returns to guest and requires
  logging in again to re-elevate.
- No password-change feature exists in the dashboard. The device credential is set once, in
  `secrets.yaml`, at build time (re-run `scripts/embed_dashboard.py` after changing it).

## Inactivity Timeout (Operator Only)

**Timeout duration:** 2 minutes (120 seconds) of no tracked activity.

**Activity tracking:** mouse movement, keyboard input, mouse clicks, page scrolling, touch
events (mobile). Any of these resets the timer.

**Behavior:**

1. User logs in and is elevated to operator.
2. Timer starts counting down from 2:00, shown in the header as `⏱️ Logout in 1:45`.
3. Any tracked activity resets the timer back to 2:00.
4. If the timer reaches 0:00, the dashboard logs out silently (returns to guest) — no
   banner. The header role badge flips back to guest.

Guest sessions are exempt — there is nothing to time out, since guest has no elevated access to
begin with.

## Security Model — Read This Before Assuming Anything Is Enforced

- All permission checks (`currentRole !== 'operator'`) run in the browser. They gate what the
  *dashboard UI* shows/does, not what the *device* will accept. The REST API is open on the
  LAN so guests can load live metrics without a password — treat the LAN/VPN as the trust
  boundary.
- Login verifies the entered username/password against a SHA-256 token baked into the
  firmware from `secrets.yaml` at embed time. A wrong password cannot elevate the UI; a
  correct password still only unlocks the form controls, not a separate API privilege.
- Not suitable for a deployment where the network between browser and device isn't already
  trusted. This project treats that trust boundary as the LAN/VPN perimeter (see the main
  `README.md` site-to-site VPN section), not the dashboard's login screen.

### If real server-side authorization is ever needed

ESPHome's `web_server` component would need to grow multi-account/role support, or a proxy would
need to sit in front of it, before "admin" vs "operator" vs anything else could be a real
distinction. Until then, don't reintroduce role tiers in the dashboard that imply enforcement
that doesn't exist — it's actively misleading (this is exactly what went wrong before: a fake
"superadmin" tier existed for over a year with a hardcoded password that turned out to match the
real device credential, and was committed to git).

## Browser Compatibility

| Browser | Support | Notes |
|---------|---------|-------|
| Chrome/Chromium | ✅ | Full support |
| Firefox | ✅ | Full support |
| Safari | ✅ | Full support |
| Edge | ✅ | Full support |
| Mobile browsers | ✅ | Touch events tracked |

**Requirements:** JavaScript enabled. No localStorage/cookie dependency.

## Troubleshooting

### Login rejected

See `RBAC_USER_GUIDE.md` → Troubleshooting. This is almost always either a wrong
username/password or the device being unreachable from the browser.

### Timer not counting down

1. Confirm the role badge shows `Operator`, not `Guest` — guests don't get a timer.
2. Move the mouse or click to trigger an activity event.
3. Check the browser console for JavaScript errors.

### Logged out unexpectedly

- Expected after 2 minutes of no mouse/keyboard/touch activity.
- Also expected on page reload — sessions are not persisted, log in again.

## Related Documentation

- `reference/RBAC_USER_GUIDE.md` — login flow and guest/operator access levels.
- `assets/dashboard.html` — implementation.

---

**Last reviewed:** 2026-07-24 — rewritten to remove the stale three-tier auth description and
the hardcoded demo password that had leaked the real device credential.
