#include <windows.h>
#include <shellapi.h>

#define WM_TRAYICON (WM_APP + 1)
#define ID_TOGGLE_ACTIVE 1001
#define ID_POPUP_ON 1002
#define ID_POPUP_OFF 1003
#define ID_POPUP_AUTO 1004
#define ID_DESKTOP_KEEP_UP 1005
#define ID_QUIT 1006
#define ID_TIMER_CHECK 9001

bool g_isActive = true;
int g_popupSetting = 2; // 0 = Off, 1 = On, 2 = Auto
bool g_keepTaskbarOnDesktop = true; 
bool g_isCurrentlyHidden = false;
bool g_isStateInitialized = false;
int g_tempShowTicks = 0; // Countdown timer for temporary reveals (12 ticks = 3 sec)

HWINEVENTHOOK g_hHookForeground = NULL;
HWINEVENTHOOK g_hHookLocation = NULL;
NOTIFYICONDATA nid = {};
HWND g_hwndApp = NULL;

void CheckAndPromptStartup() {
    HKEY hKey;
    const char* subKey = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const char* valueName = "AutoBar";

    if (RegOpenKeyEx(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char path[MAX_PATH];
        DWORD pathLen = sizeof(path);
        LONG res = RegQueryValueEx(hKey, valueName, NULL, NULL, (LPBYTE)path, &pathLen);
        RegCloseKey(hKey);
        if (res == ERROR_SUCCESS) {
            return; // Already registered
        }
    }

    int msgBoxID = MessageBox(NULL, 
        "Would you like AutoBar to run automatically when Windows starts?", 
        "AutoBar Startup Setup", 
        MB_ICONQUESTION | MB_YESNO);

    if (msgBoxID == IDYES) {
        char exePath[MAX_PATH];
        GetModuleFileName(NULL, exePath, MAX_PATH);

        if (RegCreateKeyEx(HKEY_CURRENT_USER, subKey, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            RegSetValueEx(hKey, valueName, 0, REG_SZ, (LPBYTE)exePath, (DWORD)(strlen(exePath) + 1));
            RegCloseKey(hKey);
        }
    }
}

void ApplyTaskbarState(bool showOverlay, bool stayVisible = false, int popupSetting = 2) {
    if (!g_isActive) {
        showOverlay = true;
        stayVisible = false;
    }

    APPBARDATA abd = { sizeof(APPBARDATA) };
    abd.hWnd = FindWindow("Shell_TrayWnd", NULL);
    
    HWND trayMain = abd.hWnd;
    HWND traySec = FindWindow("Shell_SecondaryTrayWnd", NULL);

    if (stayVisible) {
        abd.lParam = ABS_ALWAYSONTOP;
        SHAppBarMessage(ABM_SETSTATE, &abd);
        if (trayMain) {
            ShowWindow(trayMain, SW_SHOW);
            SetWindowPos(trayMain, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        }
        if (traySec) {
            ShowWindow(traySec, SW_SHOW);
            SetWindowPos(traySec, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        }
        g_isCurrentlyHidden = false;
        g_isStateInitialized = true;
        return;
    }

    abd.lParam = ABS_AUTOHIDE;
    SHAppBarMessage(ABM_SETSTATE, &abd);

    if (showOverlay) {
        if (trayMain) {
            ShowWindow(trayMain, SW_SHOW);
            SetWindowPos(trayMain, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        }
        if (traySec) {
            ShowWindow(traySec, SW_SHOW);
            SetWindowPos(traySec, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        }
        g_isCurrentlyHidden = false;
    } else {
        if (popupSetting == 1) {
            if (trayMain) ShowWindow(trayMain, SW_SHOW);
            if (traySec) ShowWindow(traySec, SW_SHOW);
        } else {
            if (trayMain) ShowWindow(trayMain, SW_HIDE);
            if (traySec) ShowWindow(traySec, SW_HIDE);
        }
        g_isCurrentlyHidden = true;
    }
    g_isStateInitialized = true;
}

void CheckAndToggleTaskbar() {
    if (!g_isActive) {
        ApplyTaskbarState(true, false, g_popupSetting);
        return;
    }

    HWND hwnd = GetForegroundWindow();

    // 1. DESKTOP OVERRIDE CHECK
    bool isOnDesktop = false;
    if (!hwnd) {
        isOnDesktop = true;
    } else {
        char className[256];
        GetClassName(hwnd, className, sizeof(className));
        if (strcmp(className, "Progman") == 0 || strcmp(className, "WorkerW") == 0) {
            isOnDesktop = true;
        }
    }

    if (isOnDesktop && g_keepTaskbarOnDesktop) {
        ApplyTaskbarState(true, true, g_popupSetting);
        g_tempShowTicks = 0;
        return;
    }

    // 2. CHECK MAXIMIZED STATE
    bool isMaximized = false;
    if (hwnd) {
        char className[256];
        GetClassName(hwnd, className, sizeof(className));
        isMaximized = (IsWindowVisible(hwnd) && IsZoomed(hwnd) && !IsIconic(hwnd));
        if (strcmp(className, "Shell_TrayWnd") == 0 || strcmp(className, "Windows.UI.Core.CoreWindow") == 0) {
            isMaximized = false;
        }
    }

    // 3. POPUP MODES
    if (g_popupSetting == 0) {
        // Mode: OFF (Strictly force-hidden)
        ApplyTaskbarState(false, false, 0);
        g_tempShowTicks = 0;
    } 
    else if (g_popupSetting == 1) {
        // Mode: ON (Native auto-hide, hover always enabled)
        ApplyTaskbarState(true, false, 1);
        g_tempShowTicks = 0;
    } 
    else if (g_popupSetting == 2) {
        // Mode: AUTO
        if (isMaximized) {
            ApplyTaskbarState(false, false, 2);
            g_tempShowTicks = 0;
        } else {
            // Non-Maximized: Show for 3 seconds, then automatically hide
            ApplyTaskbarState(true, false, 2);
            g_tempShowTicks = 12; // 12 * 250ms = 3 seconds
        }
    }
}

void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, 
                           LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime) {
    if (idObject == OBJID_WINDOW && idChild == CHILDID_SELF) {
        CheckAndToggleTaskbar();
    }
}

void HookEvents() {
    if (!g_hHookForeground) g_hHookForeground = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (!g_hHookLocation) g_hHookLocation = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, NULL, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
}

void UnhookEvents() {
    if (g_hHookForeground) { UnhookWinEvent(g_hHookForeground); g_hHookForeground = NULL; }
    if (g_hHookLocation) { UnhookWinEvent(g_hHookLocation); g_hHookLocation = NULL; }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            SetTimer(hwnd, ID_TIMER_CHECK, 250, NULL);
            return 0;

        case WM_TIMER:
            if (wParam == ID_TIMER_CHECK && g_isActive) {
                // Handle 3-second temporary reveal countdown
                if (g_tempShowTicks > 0) {
                    g_tempShowTicks--;
                    if (g_tempShowTicks == 0) {
                        ApplyTaskbarState(false, false, g_popupSetting);
                    }
                } 
                // If taskbar is hidden, check hover edge for AUTO mode
                else if (g_popupSetting == 2 && g_isCurrentlyHidden) {
                    HWND fgHwnd = GetForegroundWindow();
                    bool isMaximized = false;
                    if (fgHwnd) {
                        char className[256];
                        GetClassName(fgHwnd, className, sizeof(className));
                        isMaximized = (IsWindowVisible(fgHwnd) && IsZoomed(fgHwnd) && !IsIconic(fgHwnd));
                        if (strcmp(className, "Shell_TrayWnd") == 0 || strcmp(className, "Windows.UI.Core.CoreWindow") == 0) {
                            isMaximized = false;
                        }
                    }

                    // Only allow hover popups if active window is NOT maximized
                    if (!isMaximized) {
                        POINT pt;
                        GetCursorPos(&pt);
                        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

                        if (pt.y >= screenHeight - 5) {
                            ApplyTaskbarState(true, false, 2);
                            g_tempShowTicks = 12; // Pop up for 3 seconds, then hide again
                        }
                    }
                }
            }
            return 0;

        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                HMENU hMenu = CreatePopupMenu();
                
                AppendMenu(hMenu, MF_STRING | (g_isActive ? MF_CHECKED : 0), ID_TOGGLE_ACTIVE, "Auto-Hide on Maximize");
                AppendMenu(hMenu, MF_STRING | (g_keepTaskbarOnDesktop ? MF_CHECKED : 0), ID_DESKTOP_KEEP_UP, "Keep Taskbar on Desktop");

                HMENU hSubMenu = CreatePopupMenu();
                AppendMenu(hSubMenu, MF_STRING | (g_popupSetting == 1 ? MF_CHECKED : 0), ID_POPUP_ON, "On");
                AppendMenu(hSubMenu, MF_STRING | (g_popupSetting == 0 ? MF_CHECKED : 0), ID_POPUP_OFF, "Off");
                AppendMenu(hSubMenu, MF_STRING | (g_popupSetting == 2 ? MF_CHECKED : 0), ID_POPUP_AUTO, "Auto");

                AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hSubMenu, "Popup Settings");

                AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenu(hMenu, MF_STRING, ID_QUIT, "Quit");

                SetForegroundWindow(hwnd); 
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
                DestroyMenu(hMenu);

                if (cmd == ID_TOGGLE_ACTIVE) {
                    g_isActive = !g_isActive;
                    CheckAndToggleTaskbar();
                } else if (cmd == ID_DESKTOP_KEEP_UP) {
                    g_keepTaskbarOnDesktop = !g_keepTaskbarOnDesktop;
                    CheckAndToggleTaskbar();
                } else if (cmd == ID_POPUP_ON) {
                    g_popupSetting = 1;
                    CheckAndToggleTaskbar();
                } else if (cmd == ID_POPUP_OFF) {
                    g_popupSetting = 0;
                    CheckAndToggleTaskbar();
                } else if (cmd == ID_POPUP_AUTO) {
                    g_popupSetting = 2;
                    CheckAndToggleTaskbar();
                } else if (cmd == ID_QUIT) {
                    DestroyWindow(hwnd);
                }
            }
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, ID_TIMER_CHECK);
            Shell_NotifyIcon(NIM_DELETE, &nid);
            UnhookEvents();
            ApplyTaskbarState(true, false, g_popupSetting); 
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    CheckAndPromptStartup();

    const char CLASS_NAME[] = "TaskbarHiderTrayApp";
    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    g_hwndApp = CreateWindowEx(0, CLASS_NAME, "AutoBar", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = g_hwndApp;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    strcpy(nid.szTip, "AutoBar - Taskbar Hider");
    Shell_NotifyIcon(NIM_ADD, &nid);

    HookEvents();
    CheckAndToggleTaskbar();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}