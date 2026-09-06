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

HWINEVENTHOOK g_hHookForeground = NULL;
HWINEVENTHOOK g_hHookLocation = NULL;
NOTIFYICONDATA nid = {};
HWND g_hwndApp = NULL;

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
        return;
    }

    bool isForegroundMaximized = false;
    if (hwnd) {
        char className[256];
        GetClassName(hwnd, className, sizeof(className));
        isForegroundMaximized = (IsWindowVisible(hwnd) && IsZoomed(hwnd) && !IsIconic(hwnd));
        if (isForegroundMaximized) {
            if (strcmp(className, "Shell_TrayWnd") == 0 || strcmp(className, "Windows.UI.Core.CoreWindow") == 0) {
                isForegroundMaximized = false;
            }
        }
    }

    bool showOverlay = !isForegroundMaximized;
    ApplyTaskbarState(showOverlay, false, g_popupSetting);
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
                HWND fgHwnd = GetForegroundWindow();
                bool isOnDesktop = !fgHwnd;
                if (fgHwnd) {
                    char className[256];
                    GetClassName(fgHwnd, className, sizeof(className));
                    if (strcmp(className, "Progman") == 0 || strcmp(className, "WorkerW") == 0) {
                        isOnDesktop = true;
                    }
                }

                if (isOnDesktop && g_keepTaskbarOnDesktop) {
                    HWND trayMain = FindWindow("Shell_TrayWnd", NULL);
                    if (trayMain && !IsWindowVisible(trayMain)) {
                        ShowWindow(trayMain, SW_SHOW);
                    }
                    return 0;
                }

                if (g_popupSetting == 0 || g_popupSetting == 2) {
                    if (fgHwnd) {
                        bool isMax = (IsWindowVisible(fgHwnd) && IsZoomed(fgHwnd) && !IsIconic(fgHwnd));
                        if (isMax) {
                            HWND trayMain = FindWindow("Shell_TrayWnd", NULL);
                            if (trayMain && IsWindowVisible(trayMain)) {
                                POINT pt;
                                GetCursorPos(&pt);
                                RECT rc;
                                GetWindowRect(trayMain, &rc);
                                if (!PtInRect(&rc, pt)) {
                                    ShowWindow(trayMain, SW_HIDE);
                                    HWND traySec = FindWindow("Shell_SecondaryTrayWnd", NULL);
                                    if (traySec) ShowWindow(traySec, SW_HIDE);
                                    g_isCurrentlyHidden = true;
                                }
                            }
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
    const char CLASS_NAME[] = "TaskbarHiderTrayApp";
    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    g_hwndApp = CreateWindowEx(0, CLASS_NAME, "TaskbarHider", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = g_hwndApp;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    strcpy(nid.szTip, "Maximized Window Taskbar Hider");
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