# AFC Error Handling Design

## Goal

Surface AFC errors and recovery flows to HelixScreen users through two complementary channels: native action:prompt modals (interactive recovery) and toast/notification history (status awareness).

## Context

AFC reports errors through multiple channels:
- **action:prompt protocol** — interactive dialogs with recovery buttons (e.g., "Resume", "Retry Load"). Already fully supported by HelixScreen's `ActionPromptManager` + `ActionPromptModal`.
- **message_queue** — `(message, type)` tuples exposed via the `AFC` Klipper object's `get_status()`. Contains error/warning messages with human-readable text and recovery guidance. Available through Moonraker's `notify_status_update` subscription.
- **error_state flag** — boolean indicating AFC is in an error state (print paused, awaiting intervention).
- **lane status** — per-lane state including `ERROR` state, already tracked by `AmsBackendAfc`.

HelixScreen currently tracks `error_state_` in `AmsBackendAfc` but doesn't display it. The action:prompt path works end-to-end today with no changes.

## Architecture

### Layer 1: action:prompt (primary — already working)

AFC developers use `AFCprompt.create_custom_p()` to send interactive recovery dialogs. HelixScreen's existing pipeline handles this:

```
AFC (Klipper) → action:prompt_* gcode responses
  → Moonraker notify_gcode_response
    → ActionPromptManager.process_line()
      → ActionPromptModal.show_prompt()
```

Renders as a native modal dialog with styled buttons (primary, secondary, error, warning, info). User taps a button → fires the associated G-code command → AFC processes recovery.

**No changes needed.** This is the primary channel for interactive error recovery.

### Layer 2: message queue → toast + notification history (new)

Subscribe to the `AFC` Klipper object's status updates. When the `message` field changes:

```
AFC get_status() → {"message": {"message": "text", "type": "error"}}
  → Moonraker notify_status_update
    → AmsBackendAfc::handle_status_update()
      → check dedup → toast + notification history
```

**Message handling rules:**
- `type == "error"` → error toast + notification history entry
- `type == "warning"` → warning toast + notification history entry
- Track `last_seen_message_` to prevent duplicate display (the subscription path doesn't pop from the queue, so the same message persists until replaced)

### Deduplication: action:prompt vs message queue

When AFC sends both an action:prompt AND a message_queue entry for the same error event, suppress the toast to avoid redundant UI noise. The notification history entry is always created.

**Logic:** Only suppress AFC message_queue toasts when the currently visible action:prompt is also AFC-related.

```
is_afc_prompt_active = ActionPromptManager::is_showing()
    && prompt_name contains "AFC"

if afc_prompt_active:
    → notification history only (no toast)
else:
    → toast + notification history
```

**Safe default:** If AFC doesn't include "AFC" in prompt names, no suppression occurs — worst case is a redundant toast, never a missed error.

**Timing edge case:** Message arrives before prompt → toast shows briefly, then prompt appears. Acceptable since toasts auto-dismiss in ~4 seconds.

### Error state indicator

When `error_state` flips to `true`:
- AMS panel shows error indicator on affected unit/lane (lane status already comes through as `ERROR`)
- This is visual feedback independent of toasts/modals

## Files to Modify

- **`src/printer/ams_backend_afc.cpp`** — Add message field tracking, toast/notification dispatch, dedup check
- **`include/ams_backend_afc.h`** — Add `last_seen_message_` field
- **`src/ui/action_prompt_manager.cpp`** / **`include/action_prompt_manager.h`** — Expose `is_showing()` and `current_prompt_name()` static accessors (if not already public)
- **`src/application/application.cpp`** — Ensure AFC object subscription includes the `message` field

## What We're NOT Doing

- **No ASCII diagram parsing** — AFC messages are displayed as-is. If AFC improves message formatting, we benefit automatically.
- **No special message parsing** — We don't extract lane names, commands, or structured data from message text. The message is opaque.
- **No custom error UI per error type** — All AFC errors use the same toast/history path. Differentiated recovery UX comes through action:prompt.
- **No HTTP polling of /printer/afc/status** — Moonraker subscription is sufficient and more efficient.

## Testing

- Unit tests for message dedup logic (same message ignored, new message shown)
- Unit tests for AFC prompt detection (prompt name matching)
- Integration: verify toast + history when message arrives without prompt
- Integration: verify toast suppressed when AFC prompt is active
