# Cancel E-Stop Escalation Settings

## Problem

The AbortManager always escalates a cancel to an M112 emergency stop after a hardcoded 15-second timeout. Toolchangers (and other printers with long cancel routines) get e-stopped before their cancel macro finishes, causing unnecessary firmware restarts.

## Design

### Default Behavior Change

Escalation to e-stop is **disabled by default**. When cancel is pressed, we send CANCEL_PRINT and wait indefinitely for the printer to reach a terminal state. The user can always manually e-stop via the dedicated button if truly stuck.

### Settings UI

Two new rows in the Safety section of settings_panel.xml, below the existing E-Stop Confirmation toggle:

1. **Toggle**: "Cancel Escalation" — "Escalate to emergency stop if cancel doesn't respond" (default: OFF)
2. **Dropdown** (visible only when toggle ON): "Escalation Timeout" — options: 15s, 30s, 60s, 120s (default: 30s)

The dropdown is wrapped in a container with `bind_flag_if_eq` on the toggle's subject, hidden when escalation is disabled.

### SettingsManager

New settings under `/safety/`:
- `/safety/cancel_escalation_enabled` (bool, default: `false`)
- `/safety/cancel_escalation_timeout_seconds` (int, default: `30`)

New subjects: `settings_cancel_escalation_enabled`, `settings_cancel_escalation_timeout`

### AbortManager Changes

- `CANCEL_TIMEOUT_MS` is no longer used as-is — instead, read escalation settings from SettingsManager at abort start
- When escalation is disabled: don't start the cancel timeout timer. State stays in SENT_CANCEL until print reaches terminal state.
- When escalation is enabled: start cancel timeout timer with configured value
- Probe queue timeout (2s) and heater interrupt timeout (1s) are unchanged — these are pre-cancel diagnostics

### Abort Flow (escalation OFF — new default)

```
IDLE → TRY_HEATER_INTERRUPT → PROBE_QUEUE → SENT_CANCEL → (wait indefinitely) → COMPLETE
```

### Abort Flow (escalation ON)

```
IDLE → TRY_HEATER_INTERRUPT → PROBE_QUEUE → SENT_CANCEL → (configured timeout) → SENT_ESTOP → SENT_RESTART → WAITING_RECONNECT → COMPLETE
```

## Key Files

| File | Change |
|------|--------|
| `ui_xml/settings_panel.xml` | Add toggle + conditional dropdown in Safety section |
| `src/ui/ui_panel_settings.cpp` | Add callbacks + handlers for new settings |
| `include/settings_manager.h` | Declare new getters/setters/subjects |
| `src/system/settings_manager.cpp` | Implement new settings with Config persistence |
| `include/abort_manager.h` | Remove hardcoded CANCEL_TIMEOUT_MS usage |
| `src/abort/abort_manager.cpp` | Read settings, conditionally start/skip timer |
| `tests/unit/test_abort_manager.cpp` | Test both escalation modes + configurable timeout |
