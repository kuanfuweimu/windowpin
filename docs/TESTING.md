# Validation

## Automated checks

The Release build uses MSVC `/W4 /WX`: warnings are errors.

`windowpin_tests` checks 16 cases:

1. Pin, toggle back, remove tracking.
2. Restore a window that was already topmost.
3. Independently restore mixed initial states.
4. Prune a destroyed window.
5. Reject recycled handles within the same process.
6. Reject a changed process identity.
7. Roll back tracking after a failed initial change.
8. Reject a change when its identity marker cannot be set.
9. Retain failed restores for a later retry.
10. Reject invalid handles.
11. Forced-exit cleanup removes markers without further state changes.
12. Set a real hidden HWND topmost without moving, showing or activating it.
13. Restore a native window that began topmost.
14. Drop tracking after a native window is destroyed.
15. Keep native marker namespaces independent.
16. Exclude hidden windows from picker eligibility.

`windowpin_ui_tests` creates the actual panel and controls, without showing them. It validates 12 combinations of English/Chinese, 96/144/192 DPI and minimum/default widths. Every control must exist and fit inside the client area; list and action rows must not overlap. It also checks shortcut validation and formatting. In this mode, window enumeration, timers, tray registration, global hotkeys, startup changes and settings writes are disabled.

## Local result

2026-11-11: Release built successfully with MSVC 19.41 / Windows SDK 10.0.26100.0 on Windows x64. Both CTest suites passed. All 16 core/native cases and 12 hidden layout combinations passed.

## Manual acceptance coverage still needed

The automated checks are not a full interactive compatibility test. No user desktop was controlled during development. Before treating a new release as validated for a particular workflow, manually check:

- Drag to an app, cancel with Esc, repeat the toggle, and select the same app from the list.
- Ctrl+F8 in a normal application, and changing to a shortcut already owned by another application.
- Tray open/restore/quit; close-to-tray; notification area recovery after Explorer restarts.
- Dragging across monitors with different scale factors, and moving/resizing the panel between them.
- Startup preference, then disabling it before moving/uninstalling the app.
- Normal versus elevated targets, dialogs with owners, app-specific topmost behavior.
- Keyboard tab navigation and screen-reader behavior.

Do not mark these manual scenarios passed solely because the automated build succeeds.
