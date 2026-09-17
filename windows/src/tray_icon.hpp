#pragma once
#include <windows.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <string>
#include <algorithm>
#include <vector>

#define WM_TRAYICON (WM_USER + 100)
#define ID_TRAY_EXIT 2001
#define ID_TRAY_UPDATES 2002
#define ID_TRAY_TELEMETRY 2003

class TrayIcon {
public:
    TrayIcon(HWND hWnd) : m_hWnd(hWnd) {}
    ~TrayIcon() { remove(); }

    bool init(HINSTANCE hInst) {
        m_nid.cbSize = sizeof(NOTIFYICONDATAW);
        m_nid.hWnd = m_hWnd;
        m_nid.uID = 1;
        m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        m_nid.uCallbackMessage = WM_TRAYICON;
        m_nid.hIcon = createDynamicIcon(L"?", 0, 0, false);
        wcscpy_s(m_nid.szTip, L"Ratio");
        bool res = Shell_NotifyIconW(NIM_ADD, &m_nid);
        m_nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &m_nid);
        return res;
    }

    void remove() {
        if (m_nid.hWnd) {
            Shell_NotifyIconW(NIM_DELETE, &m_nid);
            if (m_currentIcon) {
                DestroyIcon(m_currentIcon);
                m_currentIcon = nullptr;
            }
            m_nid.hWnd = nullptr;
        }
    }

    void update(const std::wstring& symbol, int createPercent, int consumePercent, 
                const std::string& state, const std::string& activeName, bool tracking) {
        if (m_currentIcon && m_lastSymbol == symbol && m_lastState == state && 
            m_lastActiveName == activeName && m_lastTracking == tracking) {
            return;
        }
        m_lastSymbol = symbol;
        m_lastState = state;
        m_lastActiveName = activeName;
        m_lastTracking = tracking;

        // Tooltip
        std::string tipStr = "Ratio · " + state + (activeName.empty() ? "" : (" · " + activeName));
        int len = MultiByteToWideChar(CP_UTF8, 0, tipStr.c_str(), -1, NULL, 0);
        std::wstring wTip(len > 0 ? len : 1, L'\0');
        if (len > 0) {
            MultiByteToWideChar(CP_UTF8, 0, tipStr.c_str(), -1, &wTip[0], len);
        }
        if (wTip.length() >= 128) wTip = wTip.substr(0, 127);
        wcscpy_s(m_nid.szTip, wTip.c_str());

        // Update icon
        HICON newIcon = createDynamicIcon(symbol, createPercent, consumePercent, tracking);
        if (newIcon) {
            if (m_currentIcon) DestroyIcon(m_currentIcon);
            m_currentIcon = newIcon;
            m_nid.hIcon = newIcon;
            m_nid.uFlags = NIF_ICON | NIF_TIP;
            Shell_NotifyIconW(NIM_MODIFY, &m_nid);
        }
    }

    void showContextMenu(bool telemetryEnabled) {
        POINT pt;
        GetCursorPos(&pt);
        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, ID_TRAY_UPDATES, L"Check for Updates…");
        
        UINT telFlags = MF_STRING | (telemetryEnabled ? MF_CHECKED : MF_UNCHECKED);
        AppendMenuW(hMenu, telFlags, ID_TRAY_TELEMETRY, L"Share Anonymous Total");
        
        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Quit Ratio");

        SetForegroundWindow(m_hWnd);
        TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, m_hWnd, NULL);
        DestroyMenu(hMenu);
    }

    RECT getTrayIconRect() {
        NOTIFYICONIDENTIFIER nid = { sizeof(NOTIFYICONIDENTIFIER) };
        nid.hWnd = m_hWnd;
        nid.uID = 1;
        RECT rect = {0};
        if (SUCCEEDED(Shell_NotifyIconGetRect(&nid, &rect))) {
            return rect;
        }
        // Fallback: cursor position
        POINT pt;
        GetCursorPos(&pt);
        rect.left = pt.x - 12;
        rect.right = pt.x + 12;
        rect.top = pt.y - 12;
        rect.bottom = pt.y + 12;
        return rect;
    }

private:
    HWND m_hWnd = nullptr;
    NOTIFYICONDATAW m_nid = {0};
    HICON m_currentIcon = nullptr;

    std::wstring m_lastSymbol;
    std::string m_lastState;
    std::string m_lastActiveName;
    bool m_lastTracking = false;

    HICON createDynamicIcon(const std::wstring& symbol, int createPercent, int consumePercent, bool tracking) {
        int iconSize = 32; // Standard High-DPI tray icon size

        using namespace Gdiplus;
        Bitmap bmp(iconSize, iconSize, PixelFormat32bppARGB);
        Graphics g(&bmp);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.Clear(Color(0, 0, 0, 0));

        // Color based on symbol
        Color color(255, 240, 240, 240); // Bright white
        if (symbol == L"↑") color = Color(255, 40, 205, 65);       // Vivid green
        else if (symbol == L"↓") color = Color(255, 255, 59, 48);   // Vivid red
        else if (symbol == L"?") color = Color(255, 255, 159, 10);  // Vivid orange
        else if (!tracking) color = Color(255, 180, 180, 180);      // Neutral light gray

        SolidBrush brush(color);

        FontFamily fontFamily(L"Segoe UI");
        Gdiplus::Font font(&fontFamily, 22.0f, FontStyleBold, UnitPixel);

        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);

        RectF r(0.0f, 0.0f, (REAL)iconSize, (REAL)iconSize);
        g.DrawString(symbol.c_str(), -1, &font, r, &format, &brush);

        HICON hIcon = nullptr;
        bmp.GetHICON(&hIcon);
        return hIcon;
    }
};
