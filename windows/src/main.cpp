#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>

#include "types.hpp"
#include "accounting.hpp"
#include "storage.hpp"
#include "process_tracker.hpp"
#include "browser_tracker.hpp"
#include "system_monitor.hpp"
#include "tray_icon.hpp"
#include "window.hpp"
#include "logger.hpp"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "wtsapi32.lib")

#define WM_BROWSER_URL_RESULT (WM_USER + 102)
#define WM_SHOW_FLYOUT (WM_USER + 105)

class RatioApp {
public:
    Ledger ledger;
    std::vector<DaySummary> history;
    std::unordered_map<std::string, std::string> rules;
    bool lightMode = false;
    bool paused = false;
    bool sleeping = false;
    bool idle = false;
    double telemetrySeconds = 0.0;
    bool telemetryEnabled = true;
    std::string installID;

    std::string activeID;
    std::string activeName = "No active app";
    std::string browserExe;
    std::string browserName;
    std::string mode;
    double activeSeconds = 0.0;

    std::chrono::system_clock::time_point lastTick;
    int ticks = 0;

    // Reset & Undo
    bool undoActive = false;
    ResetSnapshot resetUndo;
    int undoTicksRemaining = 0;

    HWND hMsgWnd = nullptr;
    std::unique_ptr<TrayIcon> trayIcon;
    std::unique_ptr<RatioWindow> ratioWindow;
    BrowserTracker browserTracker;

    RatioApp() {
        ledger.day = dayKey();
        lastTick = std::chrono::system_clock::now();
    }

    void rollover() {
        std::string today = dayKey();
        if (ledger.day != today) {
            if (ledger.create + ledger.consume > 0.0) {
                history.erase(std::remove_if(history.begin(), history.end(), 
                    [this](const DaySummary& ds) { return ds.day == ledger.day; }), history.end());
                history.push_back(DaySummary{ ledger.day, ledger.create, ledger.consume });
                std::sort(history.begin(), history.end(), [](const DaySummary& a, const DaySummary& b) {
                    return a.day > b.day;
                });
                if (history.size() > 30) history.resize(30);
            }
            ledger = Ledger();
            ledger.day = today;
            save();
        }
    }

    void save() {
        Storage::save(ledger, history, rules, lightMode, paused, telemetrySeconds, telemetryEnabled, installID);
    }

    void updateApp(const ActiveProcessInfo& proc) {
        if (proc.appID.empty()) return;
        if (activeID != proc.appID) {
            Logger::log("Foreground app switched: " + proc.appID + " (" + (proc.friendlyName.empty() ? proc.exeName : proc.friendlyName) + ")");
        }
        activeSeconds = 0.0;
        activeID = proc.appID;
        activeName = proc.friendlyName.empty() ? proc.exeName : proc.friendlyName;
        browserExe = proc.isBrowser ? proc.exeName : "";
        browserName = activeName;

        auto it = rules.find(activeID);
        if (it != rules.end()) mode = it->second;
        else mode = Classifier::appMode(activeID);

        lastTick = std::chrono::system_clock::now();
    }

    void settleTime() {
        auto now = std::chrono::system_clock::now();
        double elapsed = std::chrono::duration<double>(now - lastTick).count();
        lastTick = now;
        if (!paused && !sleeping && !idle && elapsed > 0.0 && elapsed <= 3.0) {
            ledger.record(elapsed, mode, activeID, activeName);
            if (!activeID.empty()) {
                activeSeconds += elapsed;
                telemetrySeconds += elapsed;
            }
        }
    }

    void handleBrowserUrlResult(const std::string& host) {
        if (browserExe.empty()) return;
        std::string key = host.empty() ? browserExe : ("site:" + host);
        if (key == activeID) return;

        Logger::log("Browser site detected: " + key + (host.empty() ? "" : " (hostname: " + host + ")"));

        // Settle previous context before switching to new site
        settleTime();
        activeSeconds = 0.0;
        activeID = key;
        activeName = host.empty() ? browserName : host;

        auto it = rules.find(key);
        if (it != rules.end()) mode = it->second;
        else mode = host.empty() ? Classifier::appMode(key) : Classifier::siteMode(host);

        lastTick = std::chrono::system_clock::now();
        render();
    }

    void checkBrowserTab() {
        if (browserExe.empty() || browserTracker.isBusy()) return;
        HWND hFore = GetForegroundWindow();
        if (!hFore) return;

        HWND hWndTarget = hMsgWnd;
        browserTracker.checkAsync(hFore, [hWndTarget](std::string host) {
            std::string* pStr = new std::string(host);
            if (!PostMessageW(hWndTarget, WM_BROWSER_URL_RESULT, 0, reinterpret_cast<LPARAM>(pStr))) {
                delete pStr;
            }
        });
    }

    void tick() {
        auto now = std::chrono::system_clock::now();
        double elapsed = std::chrono::duration<double>(now - lastTick).count();
        lastTick = now;

        std::string oldDay = ledger.day;
        rollover();

        idle = SystemMonitor::isUserIdle(60.0);

        if (!paused && !sleeping && !idle && oldDay == ledger.day) {
            ledger.record(elapsed, mode, activeID, activeName);
            if (elapsed > 0.0 && elapsed <= 3.0 && !activeID.empty()) {
                activeSeconds += elapsed;
                telemetrySeconds += elapsed;
            }
        }

        ticks++;
        if (ticks % 10 == 0) save();
        if (ticks % 3 == 0 && !sleeping && !paused) checkBrowserTab();

        // Check active foreground window periodically
        if (ticks % 2 == 0 && !sleeping) {
            ActiveProcessInfo current = ProcessTracker::getActiveProcess(GetCurrentProcessId());
            if (!current.appID.empty()) {
                // If it's a browser, browserTab handles site: keys
                if (!current.isBrowser && current.appID != activeID) {
                    updateApp(current);
                } else if (current.isBrowser && browserExe != current.exeName) {
                    updateApp(current);
                }
            }
        }

        // Undo timer countdown
        if (undoActive) {
            undoTicksRemaining--;
            if (undoTicksRemaining <= 0) {
                undoActive = false;
            }
        }

        render();
    }

    void chooseMode(const std::string& newMode) {
        if (activeID.empty()) return;
        settleTime();
        mode = newMode;
        rules[activeID] = newMode;
        save();
        render();
    }

    void reviewApp(const std::string& id, const std::string& newMode) {
        settleTime();
        std::string previous = "";
        auto it = rules.find(id);
        if (it != rules.end()) previous = it->second;
        else previous = Classifier::appMode(id);
        if (previous.empty() && id.rfind("site:", 0) == 0) {
            previous = Classifier::siteMode(id.substr(5));
        }

        ledger.classifyPending(id, newMode, previous);
        rules[id] = newMode;
        if (activeID == id) mode = newMode;
        save();
        render();
    }

    void togglePause() {
        settleTime();
        paused = !paused;
        lastTick = std::chrono::system_clock::now();
        save();
        render();
    }

    void resetAll() {
        if (undoActive) {
            // Restore snapshot
            ledger = resetUndo.ledger;
            rules = resetUndo.rules;
            activeSeconds = resetUndo.activeSeconds;
            paused = resetUndo.paused;
            mode = resetUndo.mode;
            undoActive = false;
            lastTick = std::chrono::system_clock::now();
            save();
            render();
            return;
        }

        settleTime();
        resetUndo = ResetSnapshot{ ledger, rules, activeSeconds, paused, {}, {}, mode };
        undoActive = true;
        undoTicksRemaining = 8; // 8-second grace period

        ledger = Ledger();
        ledger.day = dayKey();
        rules.clear();
        activeSeconds = 0.0;
        paused = false;

        auto it = Classifier::builtInApps.find(activeID);
        if (it != Classifier::builtInApps.end()) mode = it->second;
        else if (activeID.rfind("site:", 0) == 0) mode = Classifier::siteMode(activeID.substr(5));
        else mode = "";

        lastTick = std::chrono::system_clock::now();
        save();
        render();
    }

    void toggleTheme() {
        lightMode = !lightMode;
        save();
        render();
    }

    void render() {
        double total = ledger.create + ledger.consume;
        int c = total > 0.0 ? static_cast<int>(std::round((ledger.create / total) * 100.0)) : 0;
        std::string state = paused ? "PAUSED" : (sleeping || idle ? "AWAY" : (mode == "create" ? "CREATING" : (mode == "consume" ? "CONSUMING" : "UNCLASSIFIED")));
        bool tracking = !paused && !sleeping && !idle;
        std::wstring symbol = (paused || idle || sleeping) ? L"\u2161" : (mode == "create" ? L"\u2191" : (mode == "consume" ? L"\u2193" : L"?"));

        if (trayIcon) {
            trayIcon->update(symbol, c, 100 - c, state, activeName, tracking);
        }

        if (ratioWindow) {
            ratioWindow->updateData(paused, idle, sleeping, lightMode, undoActive);
        }
    }
};

static RatioApp* g_app = nullptr;
static UINT s_uTaskbarRestart = 0;

static LRESULT CALLBACK MsgWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (s_uTaskbarRestart != 0 && msg == s_uTaskbarRestart) {
        if (g_app && g_app->trayIcon) {
            g_app->trayIcon->init(GetModuleHandle(NULL));
            g_app->render();
        }
        return 0;
    }

    switch (msg) {
    case WM_TIMER:
        if (wParam == 1 && g_app) {
            g_app->tick();
        }
        return 0;

    case WM_BROWSER_URL_RESULT: {
        std::string* pStr = reinterpret_cast<std::string*>(lParam);
        if (pStr) {
            if (g_app) {
                g_app->handleBrowserUrlResult(*pStr);
            }
            delete pStr;
        }
        return 0;
    }

    case WM_SHOW_FLYOUT:
        Logger::log("Received WM_SHOW_FLYOUT. Displaying flyout window on screen.");
        if (g_app && g_app->ratioWindow) {
            RECT trayRect = g_app->trayIcon ? g_app->trayIcon->getTrayIconRect() : RECT{0,0,0,0};
            g_app->ratioWindow->showNear(trayRect);
        }
        return 0;

    case WM_TRAYICON: {
        UINT uMsg = LOWORD(lParam);
        if (uMsg == NIN_SELECT || uMsg == NIN_KEYSELECT || uMsg == WM_LBUTTONUP) {
            Logger::log("Tray icon left click / select (uMsg=" + std::to_string(uMsg) + "). Toggling flyout...");
            if (g_app && g_app->ratioWindow) {
                RECT trayRect = g_app->trayIcon ? g_app->trayIcon->getTrayIconRect() : RECT{0,0,0,0};
                g_app->ratioWindow->toggle(trayRect);
            }
        } else if (uMsg == WM_CONTEXTMENU || uMsg == WM_RBUTTONUP) {
            Logger::log("Tray icon context menu (uMsg=" + std::to_string(uMsg) + "). Showing menu...");
            if (g_app && g_app->trayIcon) {
                g_app->trayIcon->showContextMenu(g_app->telemetryEnabled);
            }
        }
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_EXIT:
            DestroyWindow(hwnd);
            return 0;
        case ID_TRAY_UPDATES:
            MessageBoxW(hwnd, L"Ratio is up to date.", L"Ratio Updates", MB_OK | MB_ICONINFORMATION);
            return 0;
        case ID_TRAY_TELEMETRY:
            if (g_app) {
                g_app->telemetryEnabled = !g_app->telemetryEnabled;
                g_app->save();
            }
            return 0;
        }
        return 0;

    case WM_POWERBROADCAST:
        if (wParam == PBT_APMSUSPEND) {
            if (g_app) { g_app->settleTime(); g_app->sleeping = true; g_app->save(); g_app->render(); }
        } else if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            if (g_app) { g_app->sleeping = false; g_app->lastTick = std::chrono::system_clock::now(); g_app->render(); }
        }
        return TRUE;

    case WM_WTSSESSION_CHANGE:
        if (wParam == WTS_SESSION_LOCK) {
            if (g_app) { g_app->settleTime(); g_app->sleeping = true; g_app->save(); g_app->render(); }
        } else if (wParam == WTS_SESSION_UNLOCK) {
            if (g_app) { g_app->sleeping = false; g_app->lastTick = std::chrono::system_clock::now(); g_app->render(); }
        }
        return 0;

    case WM_DESTROY:
        Logger::log("WM_DESTROY received in MsgWndProc.");
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR lpCmdLine, int) {
    // Check CLI flags
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 0; i < argc; ++i) {
            if (wcscmp(argv[i], L"--self-test") == 0) {
                if (AttachConsole(ATTACH_PARENT_PROCESS)) {
                    HANDLE hConOut = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
                    if (hConOut != INVALID_HANDLE_VALUE) {
                        SetStdHandle(STD_OUTPUT_HANDLE, hConOut);
                        SetStdHandle(STD_ERROR_HANDLE, hConOut);
                    }
                }
                logTest("Running Ratio Windows Self-Tests...");
                bool ok = Classifier::runSelfTest();
                logTest(ok ? "ALL TESTS PASSED" : "SELF-TEST FAILED");
                LocalFree(argv);
                return ok ? 0 : 1;
            }
        }
        LocalFree(argv);
    }

    // Mutex to ensure single instance
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Local\\VisualizeValue_Ratio_SingleInstance");
    if (!hMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        Logger::log("Another instance is already running. Signaling it to show flyout window...");
        HWND hExisting = FindWindowW(L"RatioMsgReceiver", L"RatioMsg");
        if (hExisting) {
            PostMessageW(hExisting, WM_SHOW_FLYOUT, 0, 0);
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    Logger::log("=== Ratio for Windows started (PID: " + std::to_string(GetCurrentProcessId()) + ") ===");

    // Initialize GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    RatioApp app;
    g_app = &app;

    // Load persisted data
    Storage::load(app.ledger, app.history, app.rules, app.lightMode, app.paused,
                  app.telemetrySeconds, app.telemetryEnabled, app.installID);
    Logger::log("Loaded data. Ledger day: " + app.ledger.day + ", rules count: " + std::to_string(app.rules.size()));
    app.rollover();
    app.save();

    s_uTaskbarRestart = RegisterWindowMessageW(L"TaskbarCreated");

    // Create hidden top-level window (NOT HWND_MESSAGE, allowing Shell_NotifyIcon to attach to desktop)
    WNDCLASSEXW mc = { sizeof(WNDCLASSEXW) };
    mc.lpfnWndProc = MsgWndProc;
    mc.hInstance = hInstance;
    mc.lpszClassName = L"RatioMsgReceiver";
    RegisterClassExW(&mc);

    Logger::log("Creating hidden msg window...");
    app.hMsgWnd = CreateWindowExW(0, L"RatioMsgReceiver", L"RatioMsg", WS_POPUP, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
    Logger::log("hMsgWnd created: " + std::to_string((uintptr_t)app.hMsgWnd));

    // Register session notifications
    SystemMonitor::registerSession(app.hMsgWnd);

    // Initialize Tray Icon
    Logger::log("Initializing tray icon...");
    app.trayIcon = std::make_unique<TrayIcon>(app.hMsgWnd);
    bool trayOk = app.trayIcon->init(hInstance);
    Logger::log("Tray icon init result: " + std::string(trayOk ? "true" : "false"));

    // Initialize Flyout Window
    Logger::log("Initializing flyout window...");
    app.ratioWindow = std::make_unique<RatioWindow>();
    bool winOk = app.ratioWindow->create(hInstance);
    Logger::log("Flyout window create result: " + std::string(winOk ? "true" : "false"));
    app.ratioWindow->setDataSource(&app.ledger, &app.history, &app.rules, &app.activeID, &app.activeName, &app.mode);

    // Wire up callbacks
    app.ratioWindow->onChooseMode = [&app](const std::string& m) { app.chooseMode(m); };
    app.ratioWindow->onReviewApp = [&app](const std::string& id, const std::string& m) { app.reviewApp(id, m); };
    app.ratioWindow->onTogglePause = [&app]() { app.togglePause(); };
    app.ratioWindow->onResetAll = [&app]() { app.resetAll(); };
    app.ratioWindow->onToggleTheme = [&app]() { app.toggleTheme(); };
    app.ratioWindow->onQuit = [&app]() { 
        Logger::log("onQuit invoked.");
        DestroyWindow(app.hMsgWnd); 
    };

    // Initial check of current app
    Logger::log("Checking initial active process...");
    ActiveProcessInfo initialApp = ProcessTracker::getActiveProcess(GetCurrentProcessId());
    app.updateApp(initialApp);
    app.render();

    // Pop up flyout on initial start so user sees Ratio immediately
    RECT trayRect = app.trayIcon ? app.trayIcon->getTrayIconRect() : RECT{0,0,0,0};
    app.ratioWindow->showNear(trayRect);

    // Start 1-second accounting timer
    SetTimer(app.hMsgWnd, 1, 1000, NULL);

    // Message loop
    Logger::log("Entering message loop...");
    MSG msg;
    BOOL bRet;
    while ((bRet = GetMessageW(&msg, NULL, 0, 0)) != 0) {
        if (bRet == -1) {
            Logger::log("GetMessage returned -1, error: " + std::to_string(GetLastError()));
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    Logger::log("Exited message loop, msg=" + std::to_string(msg.message) + " wParam=" + std::to_string(msg.wParam));

    // Cleanup
    SystemMonitor::unregisterSession(app.hMsgWnd);
    app.save();
    app.trayIcon.reset();
    app.ratioWindow.reset();

    Gdiplus::GdiplusShutdown(gdiplusToken);
    CloseHandle(hMutex);
    return 0;
}
