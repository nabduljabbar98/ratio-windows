#pragma once
#include <windows.h>
#include <wtsapi32.h>

#pragma comment(lib, "wtsapi32.lib")

class SystemMonitor {
public:
    static double getSecondsSinceLastInput() {
        LASTINPUTINFO lii;
        lii.cbSize = sizeof(LASTINPUTINFO);
        if (GetLastInputInfo(&lii)) {
            DWORD currentTick = GetTickCount();
            DWORD elapsed = currentTick - lii.dwTime;
            return static_cast<double>(elapsed) / 1000.0;
        }
        return 0.0;
    }

    static bool isUserIdle(double thresholdSeconds = 60.0) {
        return getSecondsSinceLastInput() >= thresholdSeconds;
    }

    static void registerSession(HWND hwnd) {
        WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION);
    }

    static void unregisterSession(HWND hwnd) {
        WTSUnRegisterSessionNotification(hwnd);
    }
};
