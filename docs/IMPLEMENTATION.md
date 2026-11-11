# Implementation notes

## Reference analysis

The reference executable identifies itself as Skybn Software, product version 3.8. Read-only inspection of version metadata, visible resource strings, and import names found the Ctrl+F8 toggle, shortcut settings, tray/startup controls, and Win32 `SetWindowPos` / `RegisterHotKey` imports. Skybn's public description documents dragging a target onto a window, a toggle shortcut, tray operation and startup preferences.

This project implements that functional contract from public Windows APIs. The original program was not launched or modified, and its license state was not changed. No machine code, decompiled code, binary resources or original icon is distributed. The original licensing and purchase features are outside this implementation. The project uses its own name and an original icon.

## Structure

- `src/core.h`: session change tracking, original-state restoration, failure/retry behavior, and stale-handle pruning.
- `src/windows.cpp`: native window inspection, instance markers, enumeration and topmost changes.
- `src/main.cpp`: native controls, drag capture, hotkeys, notification-area icon, language and portable settings.
- `tests/tests.cpp`: deterministic state tests and real hidden-window integration checks.
- `tests/ui_tests.cpp`: the actual control tree created invisibly with shell/global side effects disabled.

The visible list refreshes every two seconds while the panel is visible. Unchanged data is not rebuilt. There is no polling that continually forces a pinned window's z-order. While hidden, WindowPin uses the normal Windows message loop and reacts to hotkeys and tray events.

## State and identity

The first successful toggle records the root window's process ID and initial topmost flag. A per-instance window property marks that HWND. A destroyed window loses its properties, so a recycled numeric handle is not restored just because it has the same process ID. Returning to the original state removes the marker. Failed restorations remain tracked for a retry; a confirmed forced exit only discards those markers.

`SetWindowPos` uses `SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER`. Windows may propagate topmost changes through an owner/owned-window group; this tool tracks the root owner, not every child dialog independently. Other applications can change their own window state after WindowPin's operation.

Changing a shortcut first registers it with a spare ID, then unregisters the previous shortcut only after success. This avoids losing a working shortcut on a collision. `MOD_NOREPEAT` prevents repeated toggles from holding down the key. No keyboard hook is installed.

## API sources

- [Skybn's public feature description](https://www.skybn.com/window-always-on-top/)
- [SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos)
- [RegisterHotKey](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey)
- [Shell_NotifyIconW](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shell_notifyiconw)
- [SetPropW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setpropw)
