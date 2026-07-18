# Role-Based Access Control (RBAC) — User Guide
**Phase 12: Web GUI Security Implementation**

---

## Overview

The Coolroom Controller web dashboard implements **three-tier role-based access control (RBAC)** to protect sensitive operations and allow flexible user management. Three distinct access levels control what users can view and modify.

---

## Access Levels

### 🟢 GUEST (View-Only)
**Purpose**: Monitoring-only access for display in public areas or for non-technical staff.

**Visible Sections**:
- Main display: Temperature, setpoint, compressor status
- Alarms: High/low temperature, ice, no-cool alerts
- System health: WiFi signal, RS485 status, probe freshness
- Header shows "Guest" badge with lock icon

**Restricted Sections** (grayed out):
- ⛔ Settings pages (operational parameters)
- ⛔ System administration (backup/restore, logs, WiFi, hardware)

**Allowed Actions**:
- ✅ View real-time temperature data
- ✅ View alarm status and system health
- ✅ View WiFi connection status
- ✅ Auto-refresh every 10 seconds

**Denied Actions**:
- ❌ Cannot modify setpoint
- ❌ Cannot change alarm thresholds
- ❌ Cannot modify defrost schedule
- ❌ Cannot access settings or admin functions

**Example URL**:
```
http://192.168.1.100/assets/dashboard.html?role=guest
```

---

### 🟡 ADMIN (Operational Control)
**Purpose**: Day-to-day operational management by technicians and facility operators.

**Visible Sections**:
- All GUEST views
- ⚙️ Settings page (operational parameters section)
- System health status

**Restricted Sections** (grayed out):
- ⛔ System administration (backup/restore, logs, WiFi, hardware)

**Allowed Actions**:
- ✅ All GUEST actions
- ✅ Modify setpoint temperature (±5°C range typical)
- ✅ Adjust alarm thresholds (high/low delta)
- ✅ Change defrost interval and duration
- ✅ Modify compressor hysteresis
- ✅ View all operational metrics

**Denied Actions**:
- ❌ Cannot delete or restore logs
- ❌ Cannot backup/restore system settings
- ❌ Cannot change WiFi SSID or password
- ❌ Cannot access hardware configuration
- ❌ Cannot modify startup grace periods or lockout timers

**Example URL**:
```
http://192.168.1.100/assets/dashboard.html?role=admin
```

**Typical Use Cases**:
- Setting daily operating temperature
- Adjusting alarm thresholds during commissioning
- Monitoring compressor performance
- Checking probe health during maintenance

---

### 🔴 SUPERADMIN (Full Control)
**Purpose**: System administration and complete configuration access.

**Visible Sections**:
- All GUEST and ADMIN views
- 🔐 System administration panel (backup/restore, logs management, WiFi, hardware)

**Allowed Actions**:
- ✅ All ADMIN actions
- ✅ Backup system settings to JSON file
- ✅ Restore settings from backup file
- ✅ Delete old log files from SD card
- ✅ Download all logs as archive
- ✅ Configure WiFi SSID and password
- ✅ Access hardware configuration (GPIO, RTC, RS485 addresses)
- ✅ Modify advanced control parameters

**Example URL**:
```
http://192.168.1.100/assets/dashboard.html?role=superadmin
```

**Typical Use Cases**:
- Initial system setup and configuration
- WiFi network changes
- Backup before firmware updates
- System recovery and restoration
- Advanced troubleshooting and diagnostics
- Log archival and cleanup

---

## Implementation

### Architecture

The RBAC system is implemented entirely **client-side** in the dashboard HTML/JavaScript:

```javascript
// Permission matrix defined in JavaScript
const ROLE_PERMISSIONS = {
    guest: {
        name: 'Guest',
        canView: ['main_display', 'health_status'],
        canModify: [],
        canAdmin: false
    },
    admin: {
        name: 'Administrator',
        canView: ['main_display', 'health_status', 'settings'],
        canModify: ['setpoint', 'alarms', 'defrost', 'hysteresis'],
        canAdmin: false
    },
    superadmin: {
        name: 'Super Administrator',
        canView: ['main_display', 'health_status', 'settings', 'admin'],
        canModify: ['setpoint', 'alarms', 'defrost', 'hysteresis', 'all'],
        canAdmin: true
    }
};
```

### How Role is Determined

1. **URL Parameter** (highest priority):
   ```
   ?role=admin
   ?role=superadmin
   ?role=guest
   ```

2. **LocalStorage** (persistent within browser):
   ```javascript
   localStorage.getItem('currentUser')
   localStorage.getItem('currentRole')
   ```

3. **Default**: Falls back to `guest` if no role specified

### Authorization Flow

```
User Accesses Dashboard
    ↓
JavaScript reads role from URL/localStorage
    ↓
Permission matrix applied to UI
    ↓
Restricted sections hidden/grayed
    ↓
Buttons disabled based on role
    ↓
User attempts sensitive action
    ↓
canPerformAction() check runs
    ↓
If denied: Warning alert shown
If allowed: Operation proceeds (stub functions in Phase 12)
```

### Permission Check Function

```javascript
function canPerformAction(action) {
    const roleInfo = ROLE_PERMISSIONS[currentRole];
    return roleInfo.canModify.includes(action) || 
           roleInfo.canModify.includes('all');
}

// Usage in button handlers:
async function updateSetpoint() {
    if (!canPerformAction('setpoint')) {
        showAlert('warning', 'Insufficient permissions');
        return;
    }
    // Proceed with operation
}
```

---

## Security Considerations

### Current Implementation (Phase 12)

- ✅ Client-side permission matrix protects UI
- ✅ Role badges and visual indicators show access level
- ✅ Permission-denied warnings alert unauthorized attempts
- ✅ Button disable/enable based on role
- ✅ Section visibility controlled by role

### Important Limitations

⚠️ **Client-side security ONLY**:
- Role is passed via URL parameter (visible to user)
- Determined by JavaScript in browser
- **NOT** verified by device/server
- User could theoretically modify browser JavaScript to change role

⚠️ **Intended Use**:
This RBAC implementation is designed for **operational workflow control**, not **cryptographic security**. It prevents accidental misuse and guides users to appropriate features, but does not protect against determined attackers who can modify client-side JavaScript.

### Future Enhancement (Phase 13+)

For production/sensitive deployments, implement:
1. **Server-side authentication** via Home Assistant or custom API
2. **JWT tokens** with role claims
3. **Device-side role verification** before allowing sensitive operations
4. **Audit logging** of who changed what and when
5. **IP-based access control** for admin/superadmin roles

---

## Usage Guide

### Accessing the Dashboard

#### Basic Access (GUEST):
```bash
http://192.168.1.100/assets/dashboard.html
# or
http://192.168.1.100/assets/dashboard.html?role=guest
```

#### Admin Access:
```bash
http://192.168.1.100/assets/dashboard.html?role=admin
```

#### SuperAdmin Access:
```bash
http://192.168.1.100/assets/dashboard.html?role=superadmin
```

#### With Custom Host:
```bash
# If serving dashboard on separate web server:
https://dashboard.example.com/coolroom.html?role=admin
# Make sure dashboard.html updates fetch URL to device IP:
const API_URL = 'http://192.168.1.100/api/states';
```

### Testing Roles

1. Open dashboard in browser DevTools (F12)
2. Navigate to each role to verify:
   - GUEST: Only main display visible
   - ADMIN: Settings section appears, admin section hidden
   - SUPERADMIN: Admin section appears, all buttons enabled

3. Verify warnings appear when clicking restricted buttons as GUEST

### Persistent Role Configuration

To remember role across browser sessions:

```javascript
// In browser console:
localStorage.setItem('currentUser', 'john_admin');
localStorage.setItem('currentRole', 'admin');

// Reload dashboard - role will persist
```

---

## User Scenarios

### Scenario 1: Public Display Area
```
Location: Walk-in cooler entrance
Display: Dashboard on wall monitor
Access: GUEST role
Purpose: Show current temperature and alarm status
Security: View-only, no accidental modifications possible
URL: http://192.168.1.100/assets/dashboard.html?role=guest
```

### Scenario 2: Operator Station
```
Location: Technical control room
User: Shift operator
Access: ADMIN role
Purpose: Daily monitoring and operational adjustments
Allowed: Setpoint changes, alarm threshold adjustments
Blocked: System administration, WiFi reconfiguration
URL: http://192.168.1.100/assets/dashboard.html?role=admin
```

### Scenario 3: System Administrator
```
Location: Remote access via VPN
User: Facility engineer
Access: SUPERADMIN role
Purpose: System maintenance, backup, hardware configuration
Allowed: Everything including logs, WiFi, hardware settings
Blocked: Nothing (full access)
URL: https://dashboard.example.com/?role=superadmin
```

---

## Feature Matrix

| Feature | GUEST | ADMIN | SUPERADMIN |
|---------|-------|-------|-----------|
| View temperature | ✅ | ✅ | ✅ |
| View alarms | ✅ | ✅ | ✅ |
| View WiFi status | ✅ | ✅ | ✅ |
| View system health | ✅ | ✅ | ✅ |
| Modify setpoint | ❌ | ✅ | ✅ |
| Modify alarm thresholds | ❌ | ✅ | ✅ |
| Modify defrost schedule | ❌ | ✅ | ✅ |
| Modify hysteresis | ❌ | ✅ | ✅ |
| Backup settings | ❌ | ❌ | ✅ |
| Restore settings | ❌ | ❌ | ✅ |
| Delete logs | ❌ | ❌ | ✅ |
| Download logs | ❌ | ❌ | ✅ |
| Configure WiFi | ❌ | ❌ | ✅ |
| Hardware settings | ❌ | ❌ | ✅ |

---

## Troubleshooting

### Problem: All buttons disabled (can't make changes as ADMIN)
**Solution**: Check URL has `?role=admin` parameter. If missing, add it and reload.

### Problem: Admin section not visible as SUPERADMIN
**Solution**: Browser may have cached old dashboard. Clear cache and reload (Ctrl+F5 or Cmd+Shift+R).

### Problem: Role badge shows "GUEST" when should be "ADMIN"
**Solution**: 
1. Check URL parameters
2. Clear localStorage: `localStorage.clear()` in console
3. Reload dashboard with role parameter

### Problem: Settings changes aren't being saved
**Solution**: In Phase 12, save operations are stubs (show alert only). Phase 13 will implement actual API calls to `/api/number/*/set` endpoints.

---

## API Reference (Phase 13+)

Future API endpoints for role-based operations:

```bash
# Update setpoint (ADMIN+)
POST /api/number/ctl_setpoint/set?value=15.5

# Update alarm threshold (ADMIN+)
POST /api/number/ctl_alarm_high_delta/set?value=3.0

# Backup settings (SUPERADMIN+)
GET /api/backup/download

# Restore settings (SUPERADMIN+)
POST /api/backup/restore (multipart form data with backup.json)

# Delete logs (SUPERADMIN+)
POST /api/logs/delete?age_days=30

# WiFi configuration (SUPERADMIN+)
POST /api/wifi/config?ssid=MyNet&password=...

# Hardware settings (SUPERADMIN+)
GET /api/hardware/config
POST /api/hardware/update
```

---

## Related Documentation

- `HANDOVER_NOTES_2026-07-18.md` — Phase 12 implementation details
- `esp32-p4-coolroom.yaml` — Web server configuration comments
- `assets/dashboard.html` — JavaScript RBAC implementation source

---

**Last Updated**: 2026-07-18  
**Phase**: 12 (Web GUI Role-Based Access Control)  
**Status**: Complete — Ready for deployment
