# AutoBar

A lightweight C++ system tray application that automatically manages your Windows taskbar behavior when working with maximized or fullscreen windows, delivering an immersive, distraction-free workspace. Features fine-grained popup controls, desktop persistence, smart hover reveal, and automated Windows startup configuration.

---

## Key Features

- **Multi-Window & Fullscreen Detection:** Uses global window scanning (`EnumWindows`) and monitor-boundary calculation to auto-hide the taskbar whenever *any* window on your system is maximized or running borderless fullscreen (games, media players, IDEs).
- **Smart Auto Hover Reveal:** Temporarily reveals the taskbar for 3 seconds upon window focus or when mouse-hovering the bottom screen edge in **Auto** mode.
- **Desktop Persistence:** Option to permanently pin the taskbar whenever you return to the desktop (`Progman` / `WorkerW`), ignoring auto-hide rules.
- **Multi-Monitor Support:** Seamlessly handles both primary (`Shell_TrayWnd`) and secondary monitor taskbars (`Shell_SecondaryTrayWnd`).
- **System Tray Controls:** Easily toggle between **Off**, **On**, and **Auto** modes, configure desktop behavior, or exit via a right-click tray menu.
- **Automated Windows Startup:** Prompts on first launch to configure run-on-startup via the Windows Registry (`HKCU\...\Run`).
- **Zero Workspace Distortion:** Uses native shell state flags so Windows never distorts or resizes maximized application layouts.

---

## Version History & Changelog

### 🚀 v1.0.1 — Dawn patch01
-  **Fix:** Fixed taskbar behaver on non maximized window.


### 🚀 `v1.0.0` — Dawn (First Stable Release)
- **Production Release:** Official stable release combining global window scanning, smart auto hover reveal, desktop persistence, popup controls, and registry startup integration.
- **Global Window Scanning:** Upgraded from single-window tracking to global window enumeration (`EnumWindows`), ensuring taskbar state accurately reflects all background or foreground maximized/fullscreen windows.
- **Smart Focus & Edge Reveal:** Implemented 3-second temporary taskbar reveals upon window focus/launch and bottom edge hover triggers in Auto mode.
- **Desktop Override:** Integrated **"Keep Taskbar on Desktop"** logic (`g_keepTaskbarOnDesktop`) that forces taskbar visibility whenever shell desktop surfaces (`Progman` / `WorkerW`) are focused.
- **Startup Integration:** First-launch prompt adding background persistence via `HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run`.

---

### 🧪 Pre-Release Development Milestones

- **`v0.5` — Multi-Window & Fullscreen Engine:** Added `EnumWindows` scanning and monitor-boundary calculations for borderless fullscreen apps.
- **`v0.4` — Smart Auto Hover & Focus:** Added 3-second temporary taskbar reveal on window focus and screen edge hover events.
- **`v0.3` — Startup Configuration:** Implemented registry key checks and automated first-launch startup prompt.
- **`v0.2` — Desktop Persistence & Popup Modes:** Introduced **"Keep Taskbar on Desktop"** override along with **On**, **Off**, and **Auto** popup control menu.
- **`v0.1` — Proof of Concept:** Initial background WinEvent hook engine (`EVENT_SYSTEM_FOREGROUND`) targeting `IsZoomed` window state toggles.

---

## How to Build

Compile `autobar.cpp` using any standard C++ compiler linked with Windows API libraries (`user32`, `shell32`, `advapi32`):

### Using MinGW (GCC)
```bash
g++ autobar.cpp -o AutoBar.exe -mwindows -lshell32 -luser32 -ladvapi32