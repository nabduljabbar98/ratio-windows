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

        // Position window right above/below the tray icon
        int width = static_cast<int>(360 * m_scale);
        int height = static_cast<int>(352 * m_scale);

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
        m_visible = true;
        InvalidateRect(m_hwnd, NULL, FALSE);
    }

    void hide() {
        if (!m_hwnd || !m_visible) return;
        ShowWindow(m_hwnd, SW_HIDE);
        m_visible = false;
        m_lastHideTime = GetTickCount();
    }

    void toggle(const RECT& anchorRect) {
        DWORD now = GetTickCount();
        if (now - m_lastHideTime < 250) {
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

    bool isInteractivePoint(int x, int y) {
        if (y >= 0 && y < 44) return true; // Tabs / Header
        if (!m_showingHistory && y >= 44 && y < 88 && x >= 272) return true; // Badge
        if (m_selectedTab == 0 && !m_showingHistory && y >= 264 && y < 308) return true; // Create/Consume
        if (m_selectedTab == 2 && !m_showingHistory && y >= 88 && y < 308 && x >= 272) return true; // Review buttons
        if (y >= 308 && y < 352) return true; // Bottom Toolbar
        return false;
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
            if (LOWORD(wParam) == WA_INACTIVE) {
                hide();
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
            m_scrollOffset -= (delta / WHEEL_DELTA) * 25;
            if (m_scrollOffset < 0) m_scrollOffset = 0;
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
        // Tab Header: 0..44
        if (y >= 0 && y < 44) {
            if (x >= 0 && x < 180) {
                m_selectedTab = 0;
                m_showingHistory = false;
                InvalidateRect(m_hwnd, NULL, FALSE);
                return;
            } else if (x >= 180 && x < 360) {
                m_selectedTab = 1;
                m_showingHistory = false;
                m_scrollOffset = 0;
                InvalidateRect(m_hwnd, NULL, FALSE);
                return;
            }
        }

        // Notification badge: X: 272..360, Y: 44..88 (in header)
        if (y >= 44 && y < 88 && x >= 272 && !m_showingHistory) {
            m_reviewingPending = !m_reviewingPending;
            m_selectedTab = 2;
            m_scrollOffset = 0;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return;
        }

        // Ratio Tab mode buttons: Y: 264..308
        if (m_selectedTab == 0 && !m_showingHistory) {
            if (y >= 264 && y < 308) {
                if (x >= 0 && x < 180) {
                    if (onChooseMode) onChooseMode("create");
                    return;
                } else if (x >= 180 && x < 360) {
                    if (onChooseMode) onChooseMode("consume");
                    return;
                }
            }
        }

        // Review list buttons: Tab 2 (Review) rows
        if (m_selectedTab == 2 && !m_showingHistory) {
            // Y: 88..308
            if (y >= 88 && y < 308) {
                auto pending = getPendingList();
                int rowIdx = (y - 88 + m_scrollOffset) / 44;
                if (rowIdx >= 0 && rowIdx < static_cast<int>(pending.size())) {
                    const std::string& appKey = pending[rowIdx].first;
                    if (x >= 272 && x < 316) {
                        if (onReviewApp) onReviewApp(appKey, "create");
                        return;
                    } else if (x >= 316 && x < 360) {
                        if (onReviewApp) onReviewApp(appKey, "consume");
                        return;
                    }
                }
            }
        }

        // Bottom Toolbar: Y: 308..352
        if (y >= 308 && y < 352) {
            if (x >= 0 && x < 44) {
                // Pause/Resume
                if (onTogglePause) onTogglePause();
                return;
            } else if (x >= 44 && x < 88) {
                // History toggle
                m_showingHistory = !m_showingHistory;
                m_scrollOffset = 0;
                InvalidateRect(m_hwnd, NULL, FALSE);
                return;
            } else if (x >= 88 && x < 202) {
                // Reset / Undo
                if (onResetAll) onResetAll();
                return;
            } else if (x >= 202 && x < 316) {
                // Quit
                if (onQuit) onQuit();
                return;
            } else if (x >= 316 && x < 360) {
                // Theme toggle
                if (onToggleTheme) onToggleTheme();
                return;
            }
        }
    }

    std::vector<std::pair<std::string, AppUsage>> getPendingList() {
        std::vector<std::pair<std::string, AppUsage>> list;
        const auto& led = getLedger();
        for (const auto& [k, v] : led.apps) {
            if (!m_reviewingPending || v.unclassified >= 1.0) {
                list.emplace_back(k, v);
            }
        }
        std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
            if (a.second.lastUsed == b.second.lastUsed) return a.second.seconds > b.second.seconds;
            return a.second.lastUsed > b.second.lastUsed;
        });
        return list;
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
            Gdiplus::Font font48(monoFamily.get(), 48, FontStyleRegular, UnitPixel);
            Gdiplus::Font font10Bold(monoFamily.get(), 10, FontStyleBold, UnitPixel);

            SolidBrush textBrush(UI::panelText(m_lightMode));
            SolidBrush grayBrush(UI::grayColor());
            SolidBrush createBrush(UI::createColor());
            SolidBrush consumeBrush(UI::consumeColor());
            SolidBrush unclassifiedBrush(UI::unclassifiedColor());

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
            const auto& rul = getRules();
            const std::string& actID = getActiveID();
            const std::string& curMode = getMode();

            // 1. Top Tabs (0..44)
            if (!m_showingHistory) {
                SolidBrush tab0Bg(m_selectedTab == 0 ? UI::selectionBackground(m_lightMode) : UI::panelBackground(m_lightMode));
                g.FillRectangle(&tab0Bg, 0.0f, 0.0f, 180.0f, 44.0f);
                g.DrawString(L"RATIO", -1, &font12, RectF(0.0f, 0.0f, 180.0f, 44.0f), &centerFormat, &textBrush);

                SolidBrush tab1Bg(m_selectedTab == 1 ? UI::selectionBackground(m_lightMode) : UI::panelBackground(m_lightMode));
                g.FillRectangle(&tab1Bg, 180.0f, 0.0f, 180.0f, 44.0f);
                g.DrawString(L"APPS", -1, &font12, RectF(180.0f, 0.0f, 180.0f, 44.0f), &centerFormat, &textBrush);

                UI::drawHairline(g, 180.0f, 0.0f, 1.0f, 44.0f, m_lightMode);
                UI::drawHairline(g, 0.0f, 44.0f, 360.0f, 1.0f, m_lightMode);
            } else {
                // History Title
                g.DrawString(L"HISTORY", -1, &font12Bold, RectF(16.0f, 0.0f, 200.0f, 44.0f), &leftFormat, &textBrush);
                std::wstring countStr = std::to_wstring(hist.size() + 1) + L" DAYS";
                g.DrawString(countStr.c_str(), -1, &font12, RectF(180.0f, 0.0f, 164.0f, 44.0f), &rightFormat, &grayBrush);
                UI::drawHairline(g, 0.0f, 44.0f, 360.0f, 1.0f, m_lightMode);
            }

            // Calculations
            double total = led.create + led.consume;
            int c = total > 0.0 ? static_cast<int>(std::round((led.create / total) * 100.0)) : 0;
            std::string state = m_paused ? "PAUSED" : (m_sleeping || m_idle ? "AWAY" : (curMode == "create" ? "CREATING" : (curMode == "consume" ? "CONSUMING" : "UNCLASSIFIED")));
            bool tracking = !m_paused && !m_sleeping && !m_idle;

            // Notification button in top-right of content
            int pendingCount = 0;
            for (const auto& [k, v] : led.apps) {
                if (v.unclassified >= 1.0) pendingCount++;
            }

            if (!m_showingHistory && m_selectedTab != 1) {
                if (pendingCount > 0) {
                    SolidBrush badgeBg(UI::unclassifiedColor());
                    g.FillEllipse(&badgeBg, 324.0f, 12.0f, 20.0f, 20.0f);
                    std::wstring countText = std::to_wstring(pendingCount);
                    SolidBrush darkTxt(Color(255, 30, 30, 30));
                    g.DrawString(countText.c_str(), -1, &font10Bold, RectF(324.0f, 12.0f, 20.0f, 20.0f), &centerFormat, &darkTxt);
                } else {
                    g.DrawString(L"✓", -1, &font12, RectF(324.0f, 12.0f, 20.0f, 20.0f), &centerFormat, &grayBrush);
                }
            }

            if (m_showingHistory) {
                // 30-Day History View
                int y = 44 - m_scrollOffset;
                // Add today's entry
                std::vector<DaySummary> entries = hist;
                if (total > 0.0) {
                    entries.insert(entries.begin(), DaySummary{ "TODAY", led.create, led.consume });
                }

                if (entries.empty()) {
                    g.DrawString(L"NO HISTORY YET", -1, &font12, RectF(16.0f, 60.0f, 300.0f, 20.0f), &leftFormat, &grayBrush);
                }

                for (const auto& entry : entries) {
                    if (y >= 44 && y < 308) {
                        std::wstring wDay = formatShortDay(entry.day);
                        g.DrawString(wDay.c_str(), -1, &font12, RectF(16.0f, (REAL)y + 12.0f, 80.0f, 20.0f), &leftFormat, &grayBrush);

                        double eTotal = entry.create + entry.consume;
                        double frac = eTotal > 0.0 ? (entry.create / eTotal) : 0.5;
                        g.FillRectangle(&consumeBrush, 96.0f, (REAL)y + 20.0f, 150.0f, 4.0f);
                        g.FillRectangle(&createBrush, 96.0f, (REAL)y + 20.0f, (REAL)(150.0 * frac), 4.0f);

                        int eCreate = eTotal > 0.0 ? static_cast<int>(std::round(frac * 100.0)) : 0;
                        std::wstring rText = eTotal > 0.0 ? (std::to_wstring(eCreate) + L"/" + std::to_wstring(100 - eCreate)) : L"—/—";
                        SolidBrush* rColor = (eTotal == 0.0) ? &textBrush : (eCreate >= 50 ? &createBrush : &consumeBrush);
                        g.DrawString(rText.c_str(), -1, &font12, RectF(260.0f, (REAL)y + 12.0f, 84.0f, 20.0f), &rightFormat, rColor);

                        UI::drawHairline(g, 0.0f, (REAL)y + 44.0f, 360.0f, 1.0f, m_lightMode);
                    }
                    y += 44;
                }
            } else if (m_selectedTab == 0) {
                // RATIO VIEW
                // Status Header (44..88)
                std::string liveStr = tracking ? "TRACKING" : state;
                std::wstring wLive(liveStr.begin(), liveStr.end());
                g.DrawString(wLive.c_str(), -1, &font12, RectF(16.0f, 44.0f, 160.0f, 44.0f), &leftFormat, &grayBrush);

                double totalAppSeconds = 0.0;
                for (const auto& [k, v] : led.apps) totalAppSeconds += v.seconds;
                std::wstring wDur(formatDuration(totalAppSeconds).begin(), formatDuration(totalAppSeconds).end());
                g.DrawString(wDur.c_str(), -1, &font12, RectF(180.0f, 44.0f, 84.0f, 44.0f), &rightFormat, &grayBrush);

                UI::drawHairline(g, 0.0f, 88.0f, 360.0f, 1.0f, m_lightMode);

                // Big Ratio Number (88..180)
                std::wstring ratioStr = total > 0.0 ? (std::to_wstring(c) + L" / " + std::to_wstring(100 - c)) : L"— / —";
                g.DrawString(ratioStr.c_str(), -1, &font48, RectF(0.0f, 95.0f, 360.0f, 65.0f), &centerFormat, &textBrush);

                // Split percentage line (at Y = 175)
                double frac = total > 0.0 ? (led.create / total) : 0.5;
                g.FillRectangle(&consumeBrush, 0.0f, 175.0f, 360.0f, 2.0f);
                g.FillRectangle(&createBrush, 0.0f, 175.0f, (REAL)(360.0 * frac), 2.0f);

                // Percentage labels (180..210)
                char bufCreate[32], bufConsume[32];
                snprintf(bufCreate, sizeof(bufCreate), "↑ %.2f%% CREATING", total > 0.0 ? (led.create / total * 100.0) : 0.0);
                snprintf(bufConsume, sizeof(bufConsume), "↓ %.2f%% CONSUMING", total > 0.0 ? (led.consume / total * 100.0) : 0.0);
                std::wstring wCreate(bufCreate, bufCreate + strlen(bufCreate));
                std::wstring wConsume(bufConsume, bufConsume + strlen(bufConsume));
                g.DrawString(wCreate.c_str(), -1, &font12, RectF(16.0f, 185.0f, 160.0f, 20.0f), &leftFormat, &createBrush);
                g.DrawString(wConsume.c_str(), -1, &font12, RectF(180.0f, 185.0f, 164.0f, 20.0f), &rightFormat, &consumeBrush);

                UI::drawHairline(g, 0.0f, 215.0f, 360.0f, 1.0f, m_lightMode);

                // Explanatory Note (215..264)
                std::string noteStr = m_paused ? "Tracking paused. Click Resume to count." :
                    (m_sleeping || m_idle ? "Away · counting resumes with activity." :
                    (curMode.empty() ? "App time is counting. Choose a mode to include it in your ratio." :
                    state + " · time updates every second.\nClick a mode to correct it."));
                std::wstring wNote(noteStr.begin(), noteStr.end());
                g.DrawString(wNote.c_str(), -1, &font12, RectF(16.0f, 220.0f, 328.0f, 40.0f), &leftFormat, &textBrush);

                UI::drawHairline(g, 0.0f, 264.0f, 360.0f, 1.0f, m_lightMode);

                // Mode Buttons (264..308)
                SolidBrush createBg(curMode == "create" ? UI::selectionBackground(m_lightMode) : UI::panelBackground(m_lightMode));
                g.FillRectangle(&createBg, 0.0f, 264.0f, 180.0f, 44.0f);
                g.DrawString(L"↑ CREATE", -1, &font12Bold, RectF(0.0f, 264.0f, 180.0f, 44.0f), &centerFormat, 
                             curMode == "create" ? &createBrush : &textBrush);

                SolidBrush consumeBg(curMode == "consume" ? UI::selectionBackground(m_lightMode) : UI::panelBackground(m_lightMode));
                g.FillRectangle(&consumeBg, 180.0f, 264.0f, 180.0f, 44.0f);
                g.DrawString(L"↓ CONSUME", -1, &font12Bold, RectF(180.0f, 264.0f, 180.0f, 44.0f), &centerFormat,
                             curMode == "consume" ? &consumeBrush : &textBrush);

                UI::drawHairline(g, 180.0f, 264.0f, 1.0f, 44.0f, m_lightMode);
            } else if (m_selectedTab == 1) {
                // APPS LIST VIEW (44..308)
                int y = 44 - m_scrollOffset;
                auto pending = getPendingList();

                if (pending.empty()) {
                    g.DrawString(L"APP USE WILL APPEAR HERE", -1, &font12, RectF(16.0f, 60.0f, 300.0f, 20.0f), &leftFormat, &grayBrush);
                }

                double totalAppSeconds = 0.0;
                for (const auto& [k, v] : pending) totalAppSeconds += v.seconds;

                for (const auto& [key, usage] : pending) {
                    if (y >= 44 && y < 308) {
                        std::string name = usage.name + (key == actID ? " ·" : "");
                        std::wstring wName(name.begin(), name.end());
                        g.DrawString(wName.c_str(), -1, &font12, RectF(16.0f, (REAL)y + 10.0f, 220.0f, 20.0f), &leftFormat, &textBrush);

                        std::wstring wDur(formatDuration(usage.seconds).begin(), formatDuration(usage.seconds).end());
                        g.DrawString(wDur.c_str(), -1, &font12, RectF(240.0f, (REAL)y + 10.0f, 104.0f, 20.0f), &rightFormat, &textBrush);

                        // Relative progress bar
                        double fraction = totalAppSeconds > 0.0 ? std::min(1.0, std::max(0.0, usage.seconds / totalAppSeconds)) : 0.0;
                        SolidBrush barBrush(Color(255, 180, 180, 180));
                        g.FillRectangle(&barBrush, 16.0f, (REAL)y + 36.0f, (REAL)(232.0 * fraction), 2.0f);

                        UI::drawHairline(g, 0.0f, (REAL)y + 56.0f, 360.0f, 1.0f, m_lightMode);
                    }
                    y += 56;
                }
            } else if (m_selectedTab == 2) {
                // REVIEW PENDING VIEW (44..308)
                // Header (44..88)
                g.DrawString(m_reviewingPending ? L"TO CATEGORIZE" : L"CATEGORIZE / ACTIVITY", -1, &font12Bold, RectF(16.0f, 44.0f, 250.0f, 44.0f), &leftFormat, &textBrush);
                UI::drawHairline(g, 0.0f, 88.0f, 360.0f, 1.0f, m_lightMode);

                int y = 88 - m_scrollOffset;
                auto pending = getPendingList();

                if (pending.empty()) {
                    g.DrawString(L"All caught up.", -1, &font12, RectF(16.0f, 100.0f, 300.0f, 20.0f), &leftFormat, &grayBrush);
                }

                for (const auto& [key, usage] : pending) {
                    if (y >= 88 && y < 308) {
                        std::string assignedMode = "";
                        auto rIt = rul.find(key);
                        if (rIt != rul.end()) assignedMode = rIt->second;
                        else assignedMode = Classifier::appMode(key);
                        if (assignedMode.empty() && key.rfind("site:", 0) == 0) {
                            assignedMode = Classifier::siteMode(key.substr(5));
                        }

                        std::wstring wName(usage.name.begin(), usage.name.end());
                        SolidBrush* nameColor = assignedMode.empty() ? &unclassifiedBrush : &textBrush;
                        g.DrawString(wName.c_str(), -1, &font12, RectF(16.0f, (REAL)y + 12.0f, 140.0f, 20.0f), &leftFormat, nameColor);

                        std::wstring wDur(formatDuration(usage.seconds).begin(), formatDuration(usage.seconds).end());
                        g.DrawString(wDur.c_str(), -1, &font12, RectF(156.0f, (REAL)y + 12.0f, 108.0f, 20.0f), &rightFormat, &textBrush);

                        // ↑ Create button [272, y, 44, 44]
                        SolidBrush btnCreateBg(assignedMode == "create" ? UI::selectionBackground(m_lightMode) : UI::panelBackground(m_lightMode));
                        g.FillRectangle(&btnCreateBg, 272.0f, (REAL)y, 44.0f, 44.0f);
                        g.DrawString(L"↑", -1, &font12Bold, RectF(272.0f, (REAL)y, 44.0f, 44.0f), &centerFormat,
                                     assignedMode == "create" ? &createBrush : &grayBrush);

                        // ↓ Consume button [316, y, 44, 44]
                        SolidBrush btnConsumeBg(assignedMode == "consume" ? UI::selectionBackground(m_lightMode) : UI::panelBackground(m_lightMode));
                        g.FillRectangle(&btnConsumeBg, 316.0f, (REAL)y, 44.0f, 44.0f);
                        g.DrawString(L"↓", -1, &font12Bold, RectF(316.0f, (REAL)y, 44.0f, 44.0f), &centerFormat,
                                     assignedMode == "consume" ? &consumeBrush : &grayBrush);

                        UI::drawHairline(g, 272.0f, (REAL)y, 1.0f, 44.0f, m_lightMode);
                        UI::drawHairline(g, 316.0f, (REAL)y, 1.0f, 44.0f, m_lightMode);
                        UI::drawHairline(g, 0.0f, (REAL)y + 44.0f, 360.0f, 1.0f, m_lightMode);
                    }
                    y += 44;
                }
            }

            // 4. Bottom Toolbar (308..352)
            UI::drawHairline(g, 0.0f, 308.0f, 360.0f, 1.0f, m_lightMode);

            // Pause: [0, 308, 44, 44]
            g.DrawString(m_paused ? L"▶" : L"Ⅱ", -1, &font12, RectF(0.0f, 308.0f, 44.0f, 44.0f), &centerFormat, &textBrush);
            UI::drawHairline(g, 44.0f, 308.0f, 1.0f, 44.0f, m_lightMode);

            // History: [44, 308, 44, 44]
            if (m_showingHistory) {
                UI::drawBackArrow(g, RectF(44.0f, 308.0f, 44.0f, 44.0f), UI::panelText(m_lightMode));
            } else {
                UI::drawHistoryClock(g, RectF(44.0f, 308.0f, 44.0f, 44.0f), UI::panelText(m_lightMode));
            }
            UI::drawHairline(g, 88.0f, 308.0f, 1.0f, 44.0f, m_lightMode);

            // Reset/Undo: [88, 308, 114, 44]
            g.DrawString(m_undoActive ? L"UNDO" : L"RESET", -1, &font12, RectF(88.0f, 308.0f, 114.0f, 44.0f), &centerFormat, &textBrush);
            UI::drawHairline(g, 202.0f, 308.0f, 1.0f, 44.0f, m_lightMode);

            // Quit: [202, 308, 114, 44]
            g.DrawString(L"QUIT", -1, &font12, RectF(202.0f, 308.0f, 114.0f, 44.0f), &centerFormat, &textBrush);
            UI::drawHairline(g, 316.0f, 308.0f, 1.0f, 44.0f, m_lightMode);

            // Theme (Moon/Sun): [316, 308, 44, 44]
            if (m_lightMode) {
                UI::drawWebMoon(g, RectF(316.0f, 308.0f, 44.0f, 44.0f), UI::panelText(m_lightMode));
            } else {
                g.DrawString(L"☀", -1, &font12, RectF(316.0f, 308.0f, 44.0f, 44.0f), &centerFormat, &textBrush);
            }

            // Outer 1 physical pixel border
            Pen borderPen(UI::gridColor(m_lightMode), 1.0f);
            g.DrawRectangle(&borderPen, 0.0f, 0.0f, 359.0f, 351.0f);
        }

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
    }
};
