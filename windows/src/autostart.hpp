#pragma once
#include <windows.h>
#include <string>
#include "logger.hpp"

#pragma comment(lib, "advapi32.lib")

class AutostartManager {
public:
    static bool isAutostartEnabled() {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t path[MAX_PATH] = {0};
            DWORD size = sizeof(path);
            DWORD type = 0;
            LONG res = RegQueryValueExW(hKey, L"Ratio", NULL, &type, (LPBYTE)path, &size);
            RegCloseKey(hKey);
            return res == ERROR_SUCCESS;
        }
        return false;
    }

    static void setAutostart(bool enable) {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            if (enable) {
                wchar_t exePath[MAX_PATH] = {0};
                GetModuleFileNameW(NULL, exePath, MAX_PATH);
                std::wstring quoted = L"\"" + std::wstring(exePath) + L"\"";
                RegSetValueExW(hKey, L"Ratio", 0, REG_SZ, (const BYTE*)quoted.c_str(), (DWORD)(quoted.length() + 1) * sizeof(wchar_t));
                Logger::log("Autostart registered in HKCU Run");
            } else {
                RegDeleteValueW(hKey, L"Ratio");
                Logger::log("Autostart removed from HKCU Run");
            }
            RegCloseKey(hKey);
        }
    }
};
