# AutoBar

A lightweight C++ system tray application that automatically controls your Windows taskbar behavior when working with maximized windows, giving you a completely immersive, distraction-free workspace. Includes fine-grained control over popup modes and desktop persistence.

---

## Version History & Changelog

### `v1.3` — Clean Desktop & Persistence Update (Current)
* **New Feature:** Added the **"Keep Taskbar on Desktop"** tray option. When enabled, the taskbar stays permanently visible whenever you are on the desktop, ignoring auto-hide restrictions.
* **Refinement:** Unified all state checks to prevent taskbar flickering or unwanted pop-ups when switching between tabs.
* **States Supported:** Fully operational **On**, **Off**, and **Auto** modes.

### `v1.2` — Auto Mode Integration
* **New Feature:** Introduced **Auto** pop-up mode. 
* **Behavior:** When a window is maximized, the taskbar is forced completely hidden, blocking accidental mouse-hover triggers from popping the taskbar up while you work.

### `v1.1` — Popup Settings ("On" Mode)
* **New Feature:** Added custom tray menu controls with a **Popup Settings** submenu (`On` / `Off` / `Auto`).
* **Behavior:** In **On** mode, the application steps back and lets Windows handle native auto-hide behavior, allowing the taskbar to slide up smoothly when you hover your mouse at the edge of the screen.

### `v1.0` — Initial Release (Plain Taskbar Hide)
* **Core Logic:** Basic background window event hooks (`EVENT_SYSTEM_FOREGROUND`) to detect when a window is maximized (`IsZoomed`).
* **Core Action:** Automatically sends shell messages (`ABM_SETSTATE` and `ShowWindow`) to hide the main and secondary taskbars when working in maximized full-screen apps.

---

## Features At-A-Glance
* **System Tray Integration:** Runs quietly in the system tray with a right-click configuration menu.
* **Multi-Monitor Support:** Automatically manages both main and secondary taskbars (`Shell_TrayWnd` and `Shell_SecondaryTrayWnd`).
* **Zero Workspace Shrinking:** Uses proper shell state flags so Windows doesn't incorrectly resize your maximized application windows.

## How to Build
Compile the source code using any standard C++ compiler (like MinGW or Visual Studio) linked with the Windows API and Shell libraries:

```bash
g++ main.cpp -o AutoBar.exe -lshell32 -luser32
