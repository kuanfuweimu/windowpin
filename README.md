<img src="resources/windowpin.svg" width="72" alt="WindowPin icon">

# WindowPin

Keep the window you need in view. A small, native Windows utility for pinning application windows above ordinary windows.

[Download the latest release](https://github.com/kuanfuweimu/windowpin/releases/latest) · [简体中文](README.zh-CN.md) · [Build status](https://github.com/kuanfuweimu/windowpin/actions)

## Use it

1. Download and extract **WindowPin-1.0.0-win-x64.zip** from Releases.
2. Run **WindowPin.exe**. No installation or additional runtime is required.
3. Focus a window and press **Ctrl + F8** to toggle its on-top state.

You can also hold the blue target area and drag it onto a window, or select a window in the searchable list and choose **Toggle on top**. Double-clicking a list row does the same thing. Press **Esc** to cancel a drag.

WindowPin uses standard Windows stacking behavior. It doesn't move, resize or deliberately activate the target window. Other topmost windows can still cover it.

## Controls

| Control | Behavior |
| --- | --- |
| Ctrl + F8 | Toggle the foreground window; change this shortcut in the panel. |
| Restore all | Restore the initial topmost state of windows changed during this session. |
| Close / Hide | Keep the program running in the notification area. |
| Tray icon | Click to open; right-click for restore and quit. |
| Quit | Restore tracked windows, unregister the shortcut and exit. |
| Start when I sign in | Opt in to a per-user Windows startup entry. Off by default. |
| Start in tray | Hide the panel on a normal launch. |
| Language | Switch between English and Simplified Chinese. |

An asterisk in the list marks a window changed by this WindowPin session. If a window was already topmost before WindowPin changed it, restoring returns it to that original state. Repeating a toggle back to the original state removes it from the restore list.

The shortcut accepts Ctrl or Alt, optionally Shift, with a letter, number, or F1–F11. If another program owns the requested shortcut, WindowPin reports the conflict and preserves its previous shortcut. The original Window On Top app may already use Ctrl + F8; quit that app or choose another combination.

## Portable settings

Settings are stored in **WindowPin.ini beside the EXE**, only when you change a preference. Keep the folder in a writable location. WindowPin does not install a service, access the network, collect analytics, or persist a list of your window titles.

Optional sign-in startup uses only this application's value:

```text
HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run\WindowPin
```

Its command is the quoted executable path followed by `--tray`. Disable sign-in startup before moving or deleting the executable. To uninstall, uncheck that option, quit WindowPin, and delete its folder.

## Requirements and limits

- Windows 10 version 1607 or newer / Windows 11, x64. Built with the C++ runtime statically linked.
- Elevated applications may reject changes from a non-elevated WindowPin. Run it at the same privilege level manually if needed. It never requests elevation automatically.
- Windows applies topmost changes to some owned windows together. A picker target is normalized to its root owner; attached dialogs follow Windows' own stacking rules. Restoration tracks that root window, not independent historical states of every owned dialog.
- Exclusive fullscreen, secure desktops, protected windows, and apps that repeatedly reset their own stacking order are not guaranteed to work. WindowPin does not continuously force a window on top.
- Restoration is best effort at normal exit and sign-out. A crash, force kill or power loss cannot run cleanup. If restoration fails, quitting asks whether to leave those windows as they are.
- Releases are currently **unsigned** and may show a Windows reputation warning. Checksums accompany the release.

## Build and test

Install Visual Studio 2022 or newer with **Desktop development with C++**, the Windows SDK, and CMake 3.24 or newer. In PowerShell with CMake on PATH:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
./tools/package.ps1
```

The executable is `build/Release/WindowPin.exe`. Packages are written to `dist/`. The icon is committed; `tools/make-icon.ps1` regenerates it from original vector geometry.

Tests cover state restoration, failures, destroyed/reused handles, actual Win32 topmost flags, position/focus preservation, and hidden UI layouts at 100%, 150%, and 200% scale in both languages. Tests only create and modify their own hidden windows. They do not drive the desktop, enumerate user windows, install startup entries, or register global hotkeys. See [validation details](docs/TESTING.md).

## Origin

WindowPin is an independent implementation of the window-pinning workflow described by [Skybn Window On Top](https://www.skybn.com/window-always-on-top/). Its code and icon are original; it is not affiliated with Skybn. No original executable, artwork, decompiled implementation, license key or licensing logic is included. See [implementation notes and API references](docs/IMPLEMENTATION.md).

MIT license. Created by [Kuanfu Weimu](https://github.com/kuanfuweimu).
