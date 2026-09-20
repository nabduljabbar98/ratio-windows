#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <memory>
#include "types.hpp"
#include "ui_renderer.hpp"
#include "accounting.hpp"
#include <string>
#include <vector>
#include <functional>
#include <cmath>
#include "logger.hpp"

inline std::wstring utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    if (len <= 1) return L"";
    std::wstring w(len - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &w[0], len);
    return w;
}

class RatioWindow {
public:
    std::function<void(const std::string&)> onChooseMode;
    std::function<void(const std::string&, const std::string&)> onReviewApp;
    std::function<void()> onTogglePause;
    std::function<void()> onResetAll;
    std::function<void()> onToggleTheme;
    std::function<void()> onQuit;

    RatioWindow() {}
    ~RatioWindow() {
        if (m_hwnd) DestroyWindow(m_hwnd);
    }

    bool create(HINSTANCE hInstance) {
        m_hInstance = hInstance;
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = WndProcStatic;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"RatioFlyoutWindow";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.style = CS_DROPSHADOW;
        RegisterClassExW(&wc);

        // Compute DPI
        HDC hdc = GetDC(NULL);
        m_dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(NULL, hdc);
        m_scale = static_cast<float>(m_dpi) / 96.0f;

        int width = static_cast<int>(360 * m_scale);
        int height = static_cast<int>(352 * m_scale);

        m_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"RatioFlyoutWindow",
            L"Ratio",
            WS_POPUP,
            0, 0, width, height,
            NULL, NULL, hInstance, this
        );

        return m_hwnd != nullptr;
    }

    HWND getHwnd() const { return m_hwnd; }

    void showNear(const RECT& anchorRect) {
        if (!m_hwnd) return;
        m_showTime = GetTickCount();
        Logger::log("RatioWindow::showNear called.");

        int width = static_cast<int>(360 * m_scale);
        int height = static_cast<int>(352 * m_scale);

        if (anchorRect.left == 0 && anchorRect.right == 0 && anchorRect.top == 0 && anchorRect.bottom == 0) {
            POINT pt;
            GetCursorPos(&pt);
            HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
            MONITORINFO mi = { sizeof(MONITORINFO) };
            GetMonitorInfoW(hMon, &mi);
            int x = mi.rcWork.right - width - 16;
            int y = mi.rcWork.bottom - height - 16;
            SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, width, height, SWP_SHOWWINDOW);
            SetForegroundWindow(m_hwnd);
            SetActiveWindow(m_hwnd);
            m_visible = true;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return;
        }

        // Position window right above/below the tray icon
        HMONITOR hMon = MonitorFromRect(&anchorRect, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(MONITORINFO) };
        GetMonitorInfoW(hMon, &mi);

        int x = (anchorRect.left + anchorRect.right) / 2 - width / 2;
        int y = anchorRect.top - height - 8;

        // If tray is at top
        if (y < mi.rcWork.top) {
            y = anchorRect.bottom + 8;
        }

        // Clamp to work area
        if (x < mi.rcWork.left) x = mi.rcWork.left + 8;
        if (x + width > mi.rcWork.right) x = mi.rcWork.right - width - 8;
        if (y + height > mi.rcWork.bottom) y = mi.rcWork.bottom - height - 8;

        SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, width, height, SWP_SHOWWINDOW);
        SetForegroundWindow(m_hwnd);
        SetActiveWindow(m_hwnd);
        m_visible = true;
        InvalidateRect(m_hwnd, NULL, FALSE);
    }

    void hide() {
        if (!m_hwnd || !m_visible) return;
        Logger::log("RatioWindow::hide called.");
        ShowWindow(m_hwnd, SW_HIDE);
        m_visible = false;
        m_lastHideTime = GetTickCount();
    }

    void toggle(const RECT& anchorRect) {
        DWORD now = GetTickCount();
        Logger::log("RatioWindow::toggle called. m_visible=" + std::string(m_visible ? "true" : "false") +
                    " elapsed since show=" + std::to_string(now - m_showTime) +
                    " elapsed since hide=" + std::to_string(now - m_lastHideTime));
        if (now - m_lastHideTime < 300 || now - m_showTime < 300) {
            Logger::log("RatioWindow::toggle debounced.");
            return;
        }
        if (m_visible) hide();
        else showNear(anchorRect);
    }

    bool isVisible() const { return m_visible; }

    void setDataSource(const Ledger* ledger, const std::vector<DaySummary>* history,
                       const std::unordered_map<std::string, std::string>* rules,
                       const std::string* activeID, const std::string* activeName,
                       const std::string* mode) {
        m_pLedger = ledger;
        m_pHistory = history;
        m_pRules = rules;
        m_pActiveID = activeID;
        m_pActiveName = activeName;
        m_pMode = mode;
    }

    const Ledger& getLedger() const { static const Ledger s_empty; return m_pLedger ? *m_pLedger : s_empty; }
    const std::vector<DaySummary>& getHistory() const { static const std::vector<DaySummary> s_empty; return m_pHistory ? *m_pHistory : s_empty; }
    const std::unordered_map<std::string, std::string>& getRules() const { static const std::unordered_map<std::string, std::string> s_empty; return m_pRules ? *m_pRules : s_empty; }
    const std::string& getActiveID() const { static const std::string s_empty; return m_pActiveID ? *m_pActiveID : s_empty; }
    const std::string& getActiveName() const { static const std::string s_empty; return m_pActiveName ? *m_pActiveName : s_empty; }
    const std::string& getMode() const { static const std::string s_empty; return m_pMode ? *m_pMode : s_empty; }

    void updateData(bool paused, bool idle, bool sleeping, bool lightMode, bool undoActive) {
        m_paused = paused;
        m_idle = idle;
        m_sleeping = sleeping;
        m_lightMode = lightMode;
        m_undoActive = undoActive;

        if (m_visible && m_hwnd) {
            InvalidateRect(m_hwnd, NULL, FALSE);
        }
    }

    void setLightMode(bool lightMode) {
        m_lightMode = lightMode;
        if (m_hwnd) {
            InvalidateRect(m_hwnd, NULL, FALSE);
        }
    }

private:
    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;
    bool m_visible = false;
    int m_dpi = 96;
    float m_scale = 1.0f;

    // Data source pointers (owned by RatioApp)
    const Ledger* m_pLedger = nullptr;
    const std::vector<DaySummary>* m_pHistory = nullptr;
    const std::unordered_map<std::string, std::string>* m_pRules = nullptr;
    const std::string* m_pActiveID = nullptr;
    const std::string* m_pActiveName = nullptr;
    const std::string* m_pMode = nullptr;

    bool m_paused = false;
    bool m_idle = false;
    bool m_sleeping = false;
    bool m_lightMode = false;
    bool m_undoActive = false;

    // UI View State
    int m_selectedTab = 0; // 0: Ratio, 1: Apps, 2: Review
    bool m_showingHistory = false;
    bool m_reviewingPending = false;
    int m_scrollOffset = 0;
    DWORD m_lastHideTime = 0;
    DWORD m_showTime = 0;

    bool isInteractivePoint(int x, int y) {
        if (!m_showingHistory && y >= 44 && y < 308 && x >= 272) return true; // [↑] or [↓] buttons
        if (y >= 308 && y < 352) return true; // Bottom Toolbar
        return false;
    }

    static std::wstring formatAppName(const std::string& key, const std::string& rawName) {
        std::string name = rawName;
        if (name.empty()) {
            if (key.rfind("site:", 0) == 0) name = key.substr(5);
            else name = key;
        }
        if (name.length() > 4 && name.substr(name.length() - 4) == ".exe") {
            name = name.substr(0, name.length() - 4);
        }
        std::wstring w = utf8ToWide(name);
        for (auto& c : w) {
            c = towupper(c);
        }
        return w;
    }

    std::string getAppMode(const std::string& id) {
        const auto& rul = getRules();
        auto it = rul.find(id);
        if (it != rul.end()) return it->second;
        std::string m = Classifier::appMode(id);
        if (m.empty() && id.rfind("site:", 0) == 0) {
            m = Classifier::siteMode(id.substr(5));
        }
        return m;
    }

    std::vector<std::pair<std::string, AppUsage>> getAppList() {
        std::vector<std::pair<std::string, AppUsage>> list;
        const auto& led = getLedger();
        const std::string& actID = getActiveID();

        // Put active app first if it exists
        if (!actID.empty()) {
            auto itAct = led.apps.find(actID);
            if (itAct != led.apps.end()) {
                list.emplace_back(itAct->first, itAct->second);
            } else {
                list.emplace_back(actID, AppUsage{ getActiveName(), 0.0 });
            }
        }

        // Add other apps
        std::vector<std::pair<std::string, AppUsage>> others;
        for (const auto& [k, v] : led.apps) {
            if (k != actID) {
                others.emplace_back(k, v);
            }
        }

        std::sort(others.begin(), others.end(), [](const auto& a, const auto& b) {
            if (a.second.lastUsed == b.second.lastUsed) return a.second.seconds > b.second.seconds;
            return a.second.lastUsed > b.second.lastUsed;
        });

        for (auto& item : others) {
            list.push_back(std::move(item));
        }

        return list;
    }

    void copyShareTotal() {
        const auto& led = getLedger();
        double total = led.create + led.consume;
        int c = total > 0.0 ? static_cast<int>(std::round((led.create / total) * 100.0)) : 0;
        wchar_t buf[256];
        swprintf_s(buf, L"Ratio: \u2191 %d%% CREATING / \u2193 %d%% CONSUMING today via https://visualizevalue.com/ratio", c, 100 - c);

        if (OpenClipboard(m_hwnd)) {
            EmptyClipboard();
            size_t bytes = (wcslen(buf) + 1) * sizeof(wchar_t);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
            if (hMem) {
                memcpy(GlobalLock(hMem), buf, bytes);
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
            CloseClipboard();
        }
    }

    static std::wstring formatShortDay(const std::string& yyyymmdd) {
        if (yyyymmdd == "TODAY") return L"TODAY";
        if (yyyymmdd.length() != 10) return std::wstring(yyyymmdd.begin(), yyyymmdd.end());
        int year = 0, month = 0, day = 0;
        if (sscanf_s(yyyymmdd.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
            static const wchar_t* months[] = {
                L"JAN", L"FEB", L"MAR", L"APR", L"MAY", L"JUN",
                L"JUL", L"AUG", L"SEP", L"OCT", L"NOV", L"DEC"
            };
            if (month >= 1 && month <= 12) {
                return std::wstring(months[month - 1]) + L" " + std::to_wstring(day);
            }
        }
        return std::wstring(yyyymmdd.begin(), yyyymmdd.end());
    }

    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        RatioWindow* pThis = nullptr;
        if (msg == WM_NCCREATE) {
            pThis = static_cast<RatioWindow*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
            pThis->m_hwnd = hwnd;
        } else {
            pThis = reinterpret_cast<RatioWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }
        if (pThis) return pThis->WndProc(hwnd, msg, wParam, lParam);
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_ACTIVATE:
            Logger::log("RatioWindow::WM_ACTIVATE wParam=" + std::to_string(wParam));
            if (LOWORD(wParam) == WA_INACTIVE) {
                DWORD now = GetTickCount();
                if (now - m_showTime > 300) {
                    hide();
                } else {
                    Logger::log("RatioWindow::WM_ACTIVATE WA_INACTIVE ignored within 300ms grace period.");
                }
            }
            return 0;

        case WM_DPICHANGED: {
            m_dpi = HIWORD(wParam);
            m_scale = static_cast<float>(m_dpi) / 96.0f;
            RECT* prc = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(hwnd, NULL, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_SETCURSOR: {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            int x = static_cast<int>(pt.x / m_scale);
            int y = static_cast<int>(pt.y / m_scale);
            if (isInteractivePoint(x, y)) {
                SetCursor(LoadCursor(NULL, IDC_HAND));
                return TRUE;
            }
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            return TRUE;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            paint(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1; // Double-buffered in paint

        case WM_LBUTTONDOWN: {
            int x = static_cast<int>(LOWORD(lParam) / m_scale);
            int y = static_cast<int>(HIWORD(lParam) / m_scale);
            handleClick(x, y);
            return 0;
        }

        case WM_MOUSEWHEEL: {
            short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            m_scrollOffset -= (delta / WHEEL_DELTA) * 44;
            if (m_scrollOffset < 0) m_scrollOffset = 0;
            auto list = getAppList();
            int maxScroll = std::max(0, static_cast<int>(list.size() * 44) - 264);
            if (m_scrollOffset > maxScroll) m_scrollOffset = maxScroll;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_DESTROY:
            m_hwnd = nullptr;
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void handleClick(int x, int y) {
        // App List: Y: 44..308
        if (!m_showingHistory && y >= 44 && y < 308) {
            auto list = getAppList();
            int rowIdx = (y - 44 + m_scrollOffset) / 44;
            if (rowIdx >= 0 && rowIdx < static_cast<int>(list.size())) {
                const std::string& appKey = list[rowIdx].first;
                if (x >= 272 && x < 316) {
                    if (onReviewApp) onReviewApp(appKey, "create");
                    return;
                } else if (x >= 316 && x < 360) {
                    if (onReviewApp) onReviewApp(appKey, "consume");
                    return;
                }
            }
        }

        // Bottom Toolbar: Y: 308..352
        if (y >= 308 && y < 352) {
            if (x >= 0 && x < 60) {
                // History toggle
                m_showingHistory = !m_showingHistory;
                m_scrollOffset = 0;
                InvalidateRect(m_hwnd, NULL, FALSE);
                return;
            } else if (x >= 60 && x < 120) {
                // Pause/Resume
                if (onTogglePause) onTogglePause();
                return;
            } else if (x >= 120 && x < 300) {
                // Share Total
                copyShareTotal();
                return;
            } else if (x >= 300 && x < 360) {
                // Close flyout
                hide();
                return;
            }
        }
    }

    void paint(HDC hdc) {
        RECT clientRect;
        GetClientRect(m_hwnd, &clientRect);
        int w = clientRect.right - clientRect.left;
        int h = clientRect.bottom - clientRect.top;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

        {
            using namespace Gdiplus;
            Graphics g(memDC);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
            g.ScaleTransform(m_scale, m_scale);

            // Fill background
            SolidBrush bgBrush(UI::panelBackground(m_lightMode));
            g.FillRectangle(&bgBrush, 0.0f, 0.0f, 360.0f, 352.0f);

            // Fonts
            std::unique_ptr<FontFamily> monoFamily = std::make_unique<FontFamily>(L"Cascadia Code");
            if (monoFamily->GetLastStatus() != Ok) {
                monoFamily = std::make_unique<FontFamily>(L"Consolas");
            }
            Gdiplus::Font font12(monoFamily.get(), 12, FontStyleRegular, UnitPixel);
            Gdiplus::Font font12Bold(monoFamily.get(), 12, FontStyleBold, UnitPixel);
            Gdiplus::Font font10Bold(monoFamily.get(), 10, FontStyleBold, UnitPixel);

            SolidBrush textBrush(UI::panelText(m_lightMode));
            SolidBrush grayBrush(UI::grayColor());
            SolidBrush createBrush(UI::createColor());
            SolidBrush consumeBrush(UI::consumeColor());
            SolidBrush unclassifiedBrush(UI::unclassifiedColor());
            SolidBrush dimBrush(Color(255, 90, 90, 90));

            StringFormat centerFormat;
            centerFormat.SetAlignment(StringAlignmentCenter);
            centerFormat.SetLineAlignment(StringAlignmentCenter);

            StringFormat leftFormat;
            leftFormat.SetAlignment(StringAlignmentNear);
            leftFormat.SetLineAlignment(StringAlignmentCenter);

            StringFormat rightFormat;
            rightFormat.SetAlignment(StringAlignmentFar);
            rightFormat.SetLineAlignment(StringAlignmentCenter);

            const Ledger& led = getLedger();
            const auto& hist = getHistory();
            const std::string& actID = getActiveID();

            double total = led.create + led.consume;
            double frac = total > 0.0 ? (led.create / total) : 0.5;

            // 1. Top Header: Y = 0..44 (Screen 1)
            // Left: ↑ 58.11% CREATING
            wchar_t bufCreate[64], bufConsume[64];
            double createPct = total > 0.0 ? (led.create / total * 100.0) : 0.0;
            double consumePct = total > 0.0 ? (led.consume / total * 100.0) : 0.0;
            swprintf_s(bufCreate, L"\u2191 %.2f%% CREATING", createPct);
            swprintf_s(bufConsume, L"\u2193 %.2f%% CONSUMING", consumePct);

            g.DrawString(bufCreate, -1, &font12Bold, RectF(16.0f, 10.0f, 160.0f, 20.0f), &leftFormat, &createBrush);
            g.DrawString(bufConsume, -1, &font12Bold, RectF(180.0f, 10.0f, 164.0f, 20.0f), &rightFormat, &consumeBrush);

            // Split percentage bar (at Y = 36, height = 2px)
            g.FillRectangle(&consumeBrush, 0.0f, 36.0f, 360.0f, 2.0f);
            g.FillRectangle(&createBrush, 0.0f, 36.0f, (REAL)(360.0 * frac), 2.0f);

            // Hairline at Y = 43
            UI::drawHairline(g, 0.0f, 43.0f, 360.0f, 1.0f, m_lightMode);

            // 2. Main Content: Y = 44..308 (Height = 264px)
            if (!m_showingHistory) {
                // ACTIVITY / CATEGORIZE VIEW (Screen 1 & Screen 3)
                auto list = getAppList();
                g.SetClip(RectF(0.0f, 44.0f, 360.0f, 264.0f));

                if (list.empty()) {
                    g.DrawString(L"ACTIVITY WILL APPEAR HERE", -1, &font12, RectF(0.0f, 150.0f, 360.0f, 30.0f), &centerFormat, &grayBrush);
                }

                for (size_t i = 0; i < list.size(); ++i) {
                    int y = 44 + static_cast<int>(i) * 44 - m_scrollOffset;
                    if (y + 44 < 44 || y > 308) continue;

                    const auto& item = list[i];
                    std::string assignedMode = getAppMode(item.first);
                    std::wstring wName = formatAppName(item.first, item.second.name);
                    bool isActive = (item.first == actID);

                    // Row background
                    if (isActive) {
                        SolidBrush rowActiveBg(Color(m_lightMode ? 15 : 25, 40, 205, 65));
                        g.FillRectangle(&rowActiveBg, 0.0f, (REAL)y, 272.0f, 44.0f);
                    }

                    // Green active dot and Name
                    std::wstring displayName = (isActive ? L"\u25CF " : L"") + wName;
                    SolidBrush* nameColor = assignedMode.empty() ? &unclassifiedBrush : &textBrush;
                    g.DrawString(displayName.c_str(), -1, &font12Bold, RectF(16.0f, (REAL)y + 12.0f, 140.0f, 20.0f), &leftFormat, nameColor);

                    // Percentage or duration
                    wchar_t bufPct[32];
                    if (total > 0.0) {
                        double appPct = item.second.seconds / total * 100.0;
                        swprintf_s(bufPct, L"%.2f%%", appPct);
                    } else {
                        swprintf_s(bufPct, L"%s", utf8ToWide(formatDuration(item.second.seconds)).c_str());
                    }
                    g.DrawString(bufPct, -1, &font12, RectF(150.0f, (REAL)y + 12.0f, 114.0f, 20.0f), &rightFormat, &grayBrush);

                    // Hairline vertical divider at X = 272
                    UI::drawHairline(g, 272.0f, (REAL)y, 1.0f, 44.0f, m_lightMode);

                    // [↑] CREATE Button (272..316)
                    SolidBrush btnCreateBg(assignedMode == "create" ? UI::selectionBackground(m_lightMode) : UI::panelBackground(m_lightMode));
                    g.FillRectangle(&btnCreateBg, 272.0f, (REAL)y, 44.0f, 44.0f);
                    g.DrawString(L"\u2191", -1, &font12Bold, RectF(272.0f, (REAL)y, 44.0f, 44.0f), &centerFormat,
                                 assignedMode == "create" ? &createBrush : &dimBrush);

                    // Hairline vertical divider at X = 316
                    UI::drawHairline(g, 316.0f, (REAL)y, 1.0f, 44.0f, m_lightMode);

                    // [↓] CONSUME Button (316..360)
                    SolidBrush btnConsumeBg(assignedMode == "consume" ? UI::selectionBackground(m_lightMode) : UI::panelBackground(m_lightMode));
                    g.FillRectangle(&btnConsumeBg, 316.0f, (REAL)y, 44.0f, 44.0f);
                    g.DrawString(L"\u2193", -1, &font12Bold, RectF(316.0f, (REAL)y, 44.0f, 44.0f), &centerFormat,
                                 assignedMode == "consume" ? &consumeBrush : &dimBrush);

                    // Hairline horizontal divider
                    UI::drawHairline(g, 0.0f, (REAL)y + 44.0f, 360.0f, 1.0f, m_lightMode);
                }

                g.ResetClip();
            } else {
                // 30-DAY HISTORY VIEW (Screen 5: 04 / CHANGE)
                g.SetClip(RectF(0.0f, 44.0f, 360.0f, 264.0f));

                std::vector<DaySummary> entries = hist;
                if (total > 0.0) {
                    entries.insert(entries.begin(), DaySummary{ "TODAY", led.create, led.consume });
                }

                if (entries.empty()) {
                    g.DrawString(L"NO HISTORY YET", -1, &font12, RectF(0.0f, 150.0f, 360.0f, 30.0f), &centerFormat, &grayBrush);
                }

                for (size_t index = 0; index < entries.size(); ++index) {
                    int y = 44 + static_cast<int>(index) * 44 - m_scrollOffset;
                    if (y + 44 < 44 || y > 308) continue;

                    const auto& entry = entries[index];
                    std::wstring wDay = formatShortDay(entry.day);
                    g.DrawString(wDay.c_str(), -1, &font12, RectF(16.0f, (REAL)y + 12.0f, 64.0f, 20.0f), &leftFormat, &grayBrush);

                    double eTotal = entry.create + entry.consume;
                    double eFrac = eTotal > 0.0 ? (entry.create / eTotal) : 0.5;
                    g.FillRectangle(&consumeBrush, 84.0f, (REAL)y + 21.0f, 164.0f, 2.0f);
                    g.FillRectangle(&createBrush, 84.0f, (REAL)y + 21.0f, (REAL)(164.0 * eFrac), 2.0f);

                    int eCreate = eTotal > 0.0 ? static_cast<int>(std::round(eFrac * 100.0)) : 0;
                    std::wstring rText = eTotal > 0.0 ? (std::to_wstring(eCreate) + L"/" + std::to_wstring(100 - eCreate)) : L"\u2014/\u2014";
                    SolidBrush* rColor = (eTotal == 0.0) ? &textBrush : (eCreate >= 50 ? &createBrush : &consumeBrush);
                    g.DrawString(rText.c_str(), -1, &font12Bold, RectF(260.0f, (REAL)y + 12.0f, 84.0f, 20.0f), &rightFormat, rColor);

                    UI::drawHairline(g, 0.0f, (REAL)y + 44.0f, 360.0f, 1.0f, m_lightMode);
                }

                g.ResetClip();
            }

            // 3. Bottom Toolbar: Y = 308..352 (Height = 44px)
            UI::drawHairline(g, 0.0f, 308.0f, 360.0f, 1.0f, m_lightMode);

            // History button: [0, 308, 60, 44]
            if (m_showingHistory) {
                UI::drawBackArrow(g, RectF(0.0f, 308.0f, 60.0f, 44.0f), UI::panelText(m_lightMode));
            } else {
                UI::drawHistoryClock(g, RectF(0.0f, 308.0f, 60.0f, 44.0f), UI::panelText(m_lightMode));
            }
            UI::drawHairline(g, 60.0f, 308.0f, 1.0f, 44.0f, m_lightMode);

            // Pause: [60, 308, 60, 44]
            g.DrawString(m_paused ? L"\u25B6" : L"\u2161", -1, &font12, RectF(60.0f, 308.0f, 60.0f, 44.0f), &centerFormat, &textBrush);
            UI::drawHairline(g, 120.0f, 308.0f, 1.0f, 44.0f, m_lightMode);

            // Share: [120, 308, 180, 44]
            g.DrawString(L"SHARE \u2197", -1, &font12, RectF(120.0f, 308.0f, 180.0f, 44.0f), &centerFormat, &textBrush);
            UI::drawHairline(g, 300.0f, 308.0f, 1.0f, 44.0f, m_lightMode);

            // Close: [300, 308, 60, 44]
            g.DrawString(L"\u2715", -1, &font12, RectF(300.0f, 308.0f, 60.0f, 44.0f), &centerFormat, &textBrush);

            // Outer border
            Pen borderPen(UI::gridColor(m_lightMode), 1.0f);
            g.DrawRectangle(&borderPen, 0.0f, 0.0f, 359.0f, 351.0f);
        }

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
    }
};
