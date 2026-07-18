# User Authentication & Session Management Guide
**Phase 12b: Complete Authentication System**

---

## Overview

The Coolroom Controller web dashboard now features a **complete authentication system** with:
- ✅ Three pre-configured users with role-based access
- ✅ Secure password management (default: P@lli5ter)
- ✅ Auto-logout after 2 minutes of inactivity (non-guest users only)
- ✅ Real-time inactivity countdown display
- ✅ SuperAdmin-only password change capability
- ✅ Session persistence via browser localStorage
- ✅ Activity tracking on mouse, keyboard, and touch events

---

## User Accounts

### 🟢 GUEST (View-Only Access)

| Property | Value |
|----------|-------|
| **Username** | `guest` |
| **Password** | *None* (empty) |
| **Role** | Guest |
| **Session Timeout** | Exempt (no auto-logout) |
| **Access** | View-only main display |

**Login Steps:**
1. Enter username: `guest`
2. Leave password empty
3. Click Login

---

### 🟡 ADMIN (Operational Control)

| Property | Value |
|----------|-------|
| **Username** | `admin` |
| **Password** | `P@lli5ter` |
| **Role** | Administrator |
| **Session Timeout** | 2 minutes of inactivity |
| **Access** | Main display + settings + operational controls |

**Login Steps:**
1. Enter username: `admin`
2. Enter password: `P@lli5ter`
3. Click Login

**Features Available:**
- View temperature, alarms, system health
- Modify setpoint temperature
- Adjust alarm thresholds (high/low delta)
- Change defrost schedule
- Modify compressor hysteresis
- Auto-logout after 2 minutes idle

---

### 🔴 SUPERADMIN (Full Administrative Access)

| Property | Value |
|----------|-------|
| **Username** | `superadmin` |
| **Password** | `P@lli5ter` |
| **Role** | Super Administrator |
| **Session Timeout** | 2 minutes of inactivity |
| **Access** | All features including system administration |

**Login Steps:**
1. Enter username: `superadmin`
2. Enter password: `P@lli5ter`
3. Click Login

**Features Available:**
- All ADMIN capabilities
- System Administration panel:
  - Backup/restore system settings
  - Delete/download logs
  - WiFi configuration
  - Hardware settings
- **👥 User Management:**
  - View all users and their roles
  - Change passwords for admin/superadmin accounts
  - Only SuperAdmin can perform this operation

---

## Session Management

### Inactivity Timeout (Non-Guest Users)

**Timeout Duration:** 2 minutes (120 seconds)

**Activity Tracking:**
- Mouse movement
- Keyboard input
- Mouse clicks
- Page scrolling
- Touch events (mobile)

**Behavior:**
1. User logs in as admin/superadmin
2. Timer starts counting down from 2:00
3. Any activity resets the timer back to 2:00
4. If timer reaches 0:00:
   - Warning alert displays: "Session expired due to inactivity. Logging out..."
   - Dashboard automatically logs out after 2 seconds
   - Login modal reappears

**Display:**
- Real-time countdown shown in header: ⏱️ Logout in 1:45
- Yellow warning color indicates inactivity timer
- Updates every second
- Only visible for authenticated non-guest users

### Session Persistence

User sessions are **automatically saved** in browser localStorage:
- `authUser` — Logged-in username
- `authToken` — Session token
- `authRole` — User's role

**On Page Reload:**
1. Dashboard checks localStorage for saved session
2. If valid session found: User stays logged in (no login required)
3. If session expired or invalid: Login modal appears

**Manual Logout:**
- Click the red **🚪 Logout** button (always visible in header)
- Clears localStorage
- Removes activity listeners
- Displays login modal

---

## Password Management

### Default Passwords

```
👤 guest    — No password (leave empty)
🔑 admin    — P@lli5ter
🔴 superadmin — P@lli5ter
```

### Changing Passwords

**Only SuperAdmin users can change passwords.**

**Steps:**
1. Login as superadmin
2. Scroll to "System Administration" section
3. Click "👥 Manage Users" button
4. In the "Change User Password" section:
   - Select user from dropdown (admin or superadmin)
   - Enter new password
   - Click "Update Password"
5. Password is updated immediately
6. Close modal

**Password Change Effects:**
- New password takes effect immediately
- Saved to browser localStorage (in production, save to device)
- Other users with that account must use new password on next login
- SuperAdmin can change their own password

---

## Login Modal

### Display

The login modal appears when:
- Page first loads (no saved session)
- User clicks "Logout" button
- Session expires due to inactivity
- User's session token is invalid

### Fields

| Field | Description |
|-------|-------------|
| **Username** | Required. Enter: `guest`, `admin`, or `superadmin` |
| **Password** | Required for admin/superadmin. Empty for guest. |

### Demo Users Box

Always visible in login modal for quick reference:
```
Demo Users:
👤 guest (no password)
🔑 admin / P@lli5ter
🔴 superadmin / P@lli5ter
```

---

## Security Architecture

### Current Implementation (Phase 12b)

✅ **Client-Side Features:**
- Password verification in JavaScript
- Role-based access control
- UI show/hide based on permissions
- Button enable/disable based on role
- Session token generation
- Activity tracking

✅ **Browser Storage:**
- localStorage for session persistence
- localStorage for user database (for demo)
- Token-based session validation

⚠️ **Limitations (Important):**
- Passwords stored in browser (not cryptographically secure)
- Client-side validation only (no server verification)
- Session tokens are simple base64-encoded strings
- Not suitable for production high-security deployments

### Future Enhancement Recommendations

**Phase 13 should implement:**

1. **Server-Side Authentication:**
   - Move user database to device firmware/EEPROM
   - Implement proper password hashing (bcrypt/argon2)
   - Generate JWT tokens on device
   - Validate tokens server-side

2. **HTTPS/TLS:**
   - Encrypt all authentication traffic
   - Protect passwords in transit

3. **Audit Logging:**
   - Log all login/logout events
   - Track password changes
   - Monitor failed login attempts

4. **Advanced Features:**
   - Rate limiting on failed logins
   - Account lockout after N failed attempts
   - Session invalidation on logout
   - Concurrent session management
   - Multi-factor authentication (MFA)

---

## Activity Diagram

```
┌─────────────────────────────────────────────┐
│ User Opens Dashboard                        │
└──────────────────┬──────────────────────────┘
                   │
                   ▼
        ┌──────────────────────┐
        │ Session in Storage?  │
        └──────────┬───────────┘
              YES │    NO
                  │     │
                  ▼     ▼
           ┌─────────┐  ┌──────────┐
           │ Restore │  │ Show     │
           │ Session │  │ Login    │
           └────┬────┘  │ Modal    │
                │       └─────┬────┘
                │             │
                ▼             ▼
        ┌─────────────────────────┐
        │ User Authenticated ✓    │
        └────┬────────────────────┘
             │
             ├─ GUEST?
             │  └─ No timeout, no countdown
             │
             ├─ ADMIN/SUPERADMIN?
             │  ├─ Start 2-min inactivity timer
             │  ├─ Display countdown in header
             │  ├─ Listen for activity events
             │  │
             │  ├─ Activity Detected?
             │  │  └─ Reset timer to 2:00
             │  │
             │  └─ Timer Reaches 0:00?
             │     └─ Auto-logout
             │
             └─ Show Dashboard
```

---

## Browser Compatibility

| Browser | Support | Notes |
|---------|---------|-------|
| Chrome/Chromium | ✅ | Full support |
| Firefox | ✅ | Full support |
| Safari | ✅ | Full support |
| Edge | ✅ | Full support |
| Mobile browsers | ✅ | Touch events tracked |

**Requirements:**
- JavaScript enabled
- localStorage enabled
- Cookies enabled (optional)

---

## Troubleshooting

### Problem: Stuck on login screen after logout

**Solution:**
1. Check browser console (F12) for errors
2. Verify localStorage is enabled
3. Try clearing localStorage: `localStorage.clear()` in console
4. Refresh page

### Problem: Timer not counting down

**Solution:**
1. Verify you're logged in as admin/superadmin (not guest)
2. Try moving mouse or clicking to trigger activity event
3. Check browser console for JavaScript errors
4. Refresh and login again

### Problem: Auto-logout happens too quickly

**Solution:**
1. Verify no events are being triggered inadvertently
2. Check that mouse/keyboard are working
3. Increase timeout in code if needed (edit: `const SESSION_TIMEOUT = 2 * 60 * 1000`)

### Problem: Session doesn't persist on page reload

**Solution:**
1. Verify localStorage is enabled in browser
2. Check Privacy/Incognito mode (disables localStorage)
3. Check browser storage limits
4. Try a different browser

### Problem: Password change not working

**Solution:**
1. Verify you're logged in as superadmin
2. Select a user from dropdown (not "Select user...")
3. Enter new password (not empty)
4. Check browser console for errors
5. Try clearing localStorage and logging in again

---

## API Integration (Phase 13+)

Once server-side API is implemented, update these endpoints:

```javascript
// Current (client-side only):
POST /login
  Request: { username, password }
  Response: { token, role }

// Future (server-side validation):
POST /api/auth/login
  Request: { username, password }
  Response: { token, expiresIn, role }

POST /api/auth/logout
  Request: { token }
  Response: { status }

POST /api/auth/refresh-token
  Request: { token }
  Response: { newToken, expiresIn }

PUT /api/admin/users/:username/password
  Request: { newPassword }
  Response: { status }
  (SuperAdmin-only endpoint)
```

---

## Files Modified

- `assets/dashboard.html` — Complete authentication system + user UI
- `esp32-p4-coolroom.yaml` — No changes (external auth system)

## Build Impact

- **Firmware Size:** No change (external HTML auth)
- **RAM Usage:** 19.3% (unchanged)
- **Flash Usage:** 20.0% (unchanged)
- **Lines of Code:** +500 (JavaScript/HTML auth logic)

---

## Related Documentation

- `reference/RBAC_USER_GUIDE.md` — Role-based access control details
- `HANDOVER_NOTES_2026-07-18.md` — Phase 12 implementation overview
- `esp32-p4-coolroom.yaml` — YAML configuration (unchanged)

---

**Last Updated:** 2026-07-18  
**Phase:** 12b (Authentication & Session Management)  
**Status:** Complete — Ready for deployment
