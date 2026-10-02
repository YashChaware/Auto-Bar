#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <strsafe.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")

// Dynamic Application Info
#define APP_NAME "AutoBar"
#define APP_VERSION "1.0.0"

#define WM_TRAYICON (WM_APP + 1)
#define ID_TOGGLE_ACTIVE 1001
#define ID_POPUP_ON 1002
#define ID_POPUP_OFF 1003
#define ID_POPUP_AUTO 1004
#define ID_DESKTOP_KEEP_UP 1005
#define ID_QUIT 1006
#define ID_TIMER_CHECK 9001

// Global Settings
bool g_isActive = true;
int g_popupSetting = 2; // 0 = Off, 1 = On, 2 = Auto
bool g_keepTaskbarOnDesktop = true; 
bool g_isCurrentlyHidden = false;
int g_tempShowTicks = 0; // Countdown timer for temporary reveals

// State caching to prevent shell API spamming
int g_lastAppliedState = -1; // -1 = uninitialized, 0 = hidden, 1 = shown

HWINEVENTHOOK g_hHookForeground = NULL;
HWINEVENTHOOK g_hHookLocation = NULL;
NOTIFYICONDATA nid = {};
HWND g_hwndApp = NULL;
HICON g_hAppIcon = NULL;
HANDLE g_hMutex = NULL;

const char* REG_SETTINGS_KEY = "Software\\AutoBar";
const char* REG_RUN_KEY = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";

// --- EMBEDDED CONFIGURATION PERSISTENCE ---
void LoadSettings() {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, REG_SETTINGS_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD dwSize = sizeof(DWORD);
        DWORD dwVal = 0;

        if (RegQueryValueEx(hKey, "IsActive", NULL, NULL, (LPBYTE)&dwVal, &dwSize) == ERROR_SUCCESS) {
            g_isActive = (dwVal != 0);
        }
        if (RegQueryValueEx(hKey, "PopupSetting", NULL, NULL, (LPBYTE)&dwVal, &dwSize) == ERROR_SUCCESS) {
            g_popupSetting = (int)dwVal;
        }
        if (RegQueryValueEx(hKey, "KeepOnDesktop", NULL, NULL, (LPBYTE)&dwVal, &dwSize) == ERROR_SUCCESS) {
            g_keepTaskbarOnDesktop = (dwVal != 0);
        }
        RegCloseKey(hKey);
    }
}

void SaveSettings() {
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, REG_SETTINGS_KEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD dwActive = g_isActive ? 1 : 0;
        DWORD dwPopup = (DWORD)g_popupSetting;
        DWORD dwKeepDesk = g_keepTaskbarOnDesktop ? 1 : 0;

        RegSetValueEx(hKey, "IsActive", 0, REG_DWORD, (const BYTE*)&dwActive, sizeof(dwActive));
        RegSetValueEx(hKey, "PopupSetting", 0, REG_DWORD, (const BYTE*)&dwPopup, sizeof(dwPopup));
        RegSetValueEx(hKey, "KeepOnDesktop", 0, REG_DWORD, (const BYTE*)&dwKeepDesk, sizeof(dwKeepDesk));
        RegCloseKey(hKey);
    }
}

void CheckAndPromptStartup() {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, REG_RUN_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char path[MAX_PATH];
        DWORD pathLen = sizeof(path);
        LONG res = RegQueryValueEx(hKey, "AutoBar", NULL, NULL, (LPBYTE)path, &pathLen);
        RegCloseKey(hKey);
        if (res == ERROR_SUCCESS) return;
    }

    int msgBoxID = MessageBox(NULL, 
        "Would you like AutoBar to run automatically when Windows starts?", 
        "AutoBar Startup Setup", 
        MB_ICONQUESTION | MB_YESNO);

    if (msgBoxID == IDYES) {
        char exePath[MAX_PATH];
        GetModuleFileName(NULL, exePath, MAX_PATH);

        if (RegCreateKeyEx(HKEY_CURRENT_USER, REG_RUN_KEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            RegSetValueEx(hKey, "AutoBar", 0, REG_SZ, (const BYTE*)exePath, (DWORD)(strlen(exePath) + 1));
            RegCloseKey(hKey);
        }
    }
}

// --- EMBEDDED ICON GENERATOR ---
HICON CreateEmbeddedIcon() {
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (cx <= 0) cx = 16;
    if (cy <= 0) cy = 16;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcColor = CreateCompatibleDC(hdcScreen);
    HDC hdcMask = CreateCompatibleDC(hdcScreen);

    HBITMAP hbmColor = CreateCompatibleBitmap(hdcScreen, cx, cy);
    HBITMAP hbmMask = CreateBitmap(cx, cy, 1, 1, NULL);

    HBITMAP hOldColor = (HBITMAP)SelectObject(hdcColor, hbmColor);
    HBITMAP hOldMask = (HBITMAP)SelectObject(hdcMask, hbmMask);

    // Color background
    RECT rc = { 0, 0, cx, cy };
    HBRUSH hBgBrush = CreateSolidBrush(RGB(32, 36, 44));
    FillRect(hdcColor, &rc, hBgBrush);
    DeleteObject(hBgBrush);

    // Bottom accent bar
    RECT rcBar = { 2, cy - 5, cx - 2, cy - 2 };
    HBRUSH hBarBrush = CreateSolidBrush(RGB(0, 150, 255));
    FillRect(hdcColor, &rcBar, hBarBrush);
    DeleteObject(hBarBrush);

    // Mask setup
    SetBkColor(hdcMask, RGB(0, 0, 0));
    SetTextColor(hdcMask, RGB(255, 255, 255));
    FillRect(hdcMask, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

    SelectObject(hdcColor, hOldColor);
    SelectObject(hdcMask, hOldMask);
    DeleteDC(hdcColor);
    DeleteDC(hdcMask);
    ReleaseDC(NULL, hdcScreen);

    ICONINFO ii = { 0 };
    ii.fIcon = TRUE;
    ii.hbmMask = hbmMask;
    ii.hbmColor = hbmColor;

    HICON hIcon = CreateIconIndirect(&ii);
    DeleteObject(hbmColor);
    DeleteObject(hbmMask);

    return hIcon;
}

void ApplyTaskbarState(bool showOverlay, bool stayVisible = false, int popupSetting = 2) {
    if (!g_isActive) {
        showOverlay = true;
        stayVisible = false;
    }

    bool shouldShow = (stayVisible || showOverlay);
    int targetState = shouldShow ? 1 : 0;

    if (targetState == g_lastAppliedState && popupSetting == g_popupSetting) {
        return;
    }
    g_lastAppliedState = targetState;

    APPBARDATA abd = { sizeof(APPBARDATA) };
    abd.hWnd = FindWindow("Shell_TrayWnd", NULL);
    HWND trayMain = abd.hWnd;
    HWND traySec = FindWindow("Shell_SecondaryTrayWnd", NULL);

    if (shouldShow) {
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
        return;
    }

    abd.lParam = ABS_AUTOHIDE;
    SHAppBarMessage(ABM_SETSTATE, &abd);

    if (popupSetting == 1) {
        if (trayMain) ShowWindow(trayMain, SW_SHOW);
        if (traySec) ShowWindow(traySec, SW_SHOW);
    } else {
        if (trayMain) ShowWindow(trayMain, SW_HIDE);
        if (traySec) ShowWindow(traySec, SW_HIDE);
    }
    g_isCurrentlyHidden = true;
}

struct EnumWindowData {
    bool hasMaxOrFullscreen = false;
};

BOOL IsWindowValidAndVisible(HWND hwnd) {
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd) || IsIconic(hwnd)) {
        return FALSE;
    }

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) {
        return FALSE;
    }

    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0) {
        return FALSE;
    }

    return TRUE;
}

BOOL CALLBACK CheckOpenWindowsProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowValidAndVisible(hwnd)) return TRUE;

    EnumWindowData* data = (EnumWindowData*)lParam;

    char className[256];
    GetClassName(hwnd, className, sizeof(className));

    if (strcmp(className, "Shell_TrayWnd") == 0 || 
        strcmp(className, "Shell_SecondaryTrayWnd") == 0 || 
        strcmp(className, "TaskbarHiderTrayApp") == 0 ||
        strcmp(className, "Progman") == 0 || 
        strcmp(className, "WorkerW") == 0) {
        return TRUE;
    }

    if (IsZoomed(hwnd)) {
        data->hasMaxOrFullscreen = true;
        return FALSE;
    }

    RECT rcWindow;
    if (GetWindowRect(hwnd, &rcWindow)) {
        HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(MONITORINFO) };
        if (GetMonitorInfo(hMon, &mi)) {
            if (rcWindow.left <= mi.rcMonitor.left && rcWindow.top <= mi.rcMonitor.top &&
                rcWindow.right >= mi.rcMonitor.right && rcWindow.bottom >= mi.rcMonitor.bottom) {
                data->hasMaxOrFullscreen = true;
                return FALSE;
            }
        }
    }

    return TRUE;
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
        if (strcmp(className, "Progman") == 0 || strcmp(className, "WorkerW") == 0 || strcmp(className, "Shell_TrayWnd") == 0) {
            isOnDesktop = true;
        } else if (strcmp(className, "#32768") == 0) {
            POINT pt;
            GetCursorPos(&pt);
            HWND hwndUnder = WindowFromPoint(pt);
            char underClass[256];
            GetClassName(hwndUnder, underClass, sizeof(underClass));
            if (strcmp(underClass, "Progman") == 0 || strcmp(underClass, "WorkerW") == 0 || strcmp(underClass, "SysListView32") == 0) {
                isOnDesktop = true;
            }
        }
    }

    if (isOnDesktop && g_keepTaskbarOnDesktop) {
        ApplyTaskbarState(true, true, g_popupSetting);
        g_tempShowTicks = 0;
        return;
    }

    EnumWindowData winData = {};
    EnumWindows(CheckOpenWindowsProc, (LPARAM)&winData);

    if (g_popupSetting == 0) {
        ApplyTaskbarState(false, false, 0);
        g_tempShowTicks = 0;
    } 
    else if (g_popupSetting == 1) {
        ApplyTaskbarState(true, false, 1);
        g_tempShowTicks = 0;
    } 
    else if (g_popupSetting == 2) {
        if (g_tempShowTicks > 0) {
            ApplyTaskbarState(true, false, 2);
        } else if (winData.hasMaxOrFullscreen) {
            ApplyTaskbarState(false, false, 2);
        } else {
            ApplyTaskbarState(true, true, 2);
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
                if (g_tempShowTicks > 0) {
                    g_tempShowTicks--;
                    if (g_tempShowTicks == 0) {
                        CheckAndToggleTaskbar();
                    }
                } 
                else if (g_popupSetting == 2 && g_isCurrentlyHidden) {
                    POINT pt;
                    GetCursorPos(&pt);
                    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

                    if (pt.y >= screenHeight - 5) {
                        g_tempShowTicks = 12;
                        CheckAndToggleTaskbar();
                    }
                }
            }
            return 0;

        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                HMENU hMenu = CreatePopupMenu();
                
                // Dynamic Version Menu Header
                char szMenuHeader[64];
                StringCchPrintf(szMenuHeader, sizeof(szMenuHeader)/sizeof(szMenuHeader[0]), "%s v%s", APP_NAME, APP_VERSION);
                AppendMenu(hMenu, MF_STRING | MF_GRAYED, 0, szMenuHeader);
                AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

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
                    g_lastAppliedState = -1;
                    SaveSettings();
                    CheckAndToggleTaskbar();
                } else if (cmd == ID_DESKTOP_KEEP_UP) {
                    g_keepTaskbarOnDesktop = !g_keepTaskbarOnDesktop;
                    g_lastAppliedState = -1;
                    SaveSettings();
                    CheckAndToggleTaskbar();
                } else if (cmd == ID_POPUP_ON) {
                    g_popupSetting = 1;
                    g_lastAppliedState = -1;
                    SaveSettings();
                    CheckAndToggleTaskbar();
                } else if (cmd == ID_POPUP_OFF) {
                    g_popupSetting = 0;
                    g_lastAppliedState = -1;
                    SaveSettings();
                    CheckAndToggleTaskbar();
                } else if (cmd == ID_POPUP_AUTO) {
                    g_popupSetting = 2;
                    g_lastAppliedState = -1;
                    SaveSettings();
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
            if (g_hAppIcon) DestroyIcon(g_hAppIcon);
            if (g_hMutex) {
                ReleaseMutex(g_hMutex);
                CloseHandle(g_hMutex);
            }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hMutex = CreateMutex(NULL, TRUE, "AutoBarSingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (g_hMutex) CloseHandle(g_hMutex);
        return 0;
    }

    LoadSettings();
    CheckAndPromptStartup();

    const char CLASS_NAME[] = "TaskbarHiderTrayApp";
    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    g_hwndApp = CreateWindowEx(0, CLASS_NAME, APP_NAME, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    g_hAppIcon = CreateEmbeddedIcon();

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = g_hwndApp;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = g_hAppIcon;

    // Dynamic Tooltip Setup
    StringCchPrintf(nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]), "%s v%s - Taskbar Hider", APP_NAME, APP_VERSION);
    
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