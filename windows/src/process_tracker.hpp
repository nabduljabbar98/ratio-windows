#pragma once
#include <windows.h>
#include <psapi.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "version.lib")

struct ActiveProcessInfo {
    DWORD pid = 0;
    HWND hwnd = nullptr;
    std::string exeName;
    std::string friendlyName;
    std::string appID;
    bool isBrowser = false;
};

class ProcessTracker {
public:
    static inline const std::vector<std::string> knownBrowsers = {
        "chrome.exe", "msedge.exe", "brave.exe", "firefox.exe", "opera.exe", "vivaldi.exe"
    };

    static bool isBrowserProcess(const std::string& exeName) {
        std::string lower = exeName;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        for (const auto& b : knownBrowsers) {
            if (lower == b) return true;
        }
        return false;
    }

    static std::string getFriendlyName(const std::wstring& fullPath, const std::string& fallbackExe) {
        DWORD handle = 0;
        DWORD size = GetFileVersionInfoSizeW(fullPath.c_str(), &handle);
        if (size > 0) {
            std::vector<BYTE> buffer(size);
            if (GetFileVersionInfoW(fullPath.c_str(), handle, size, buffer.data())) {
                struct LANGANDCODEPAGE {
                    WORD wLanguage;
                    WORD wCodePage;
                } *lpTranslate;
                UINT cbTranslate = 0;

                if (VerQueryValueW(buffer.data(), L"\\VarFileInfo\\Translation", (LPVOID*)&lpTranslate, &cbTranslate) && cbTranslate >= sizeof(LANGANDCODEPAGE)) {
                    wchar_t subBlock[64];
                    swprintf_s(subBlock, L"\\StringFileInfo\\%04x%04x\\FileDescription", lpTranslate[0].wLanguage, lpTranslate[0].wCodePage);
                    LPWSTR lpBuffer = nullptr;
                    UINT dwBytes = 0;
                    if (VerQueryValueW(buffer.data(), subBlock, (LPVOID*)&lpBuffer, &dwBytes) && dwBytes > 1 && lpBuffer != nullptr) {
                        int len = WideCharToMultiByte(CP_UTF8, 0, lpBuffer, -1, NULL, 0, NULL, NULL);
                        if (len > 0) {
                            std::string res(len - 1, 0);
                            WideCharToMultiByte(CP_UTF8, 0, lpBuffer, -1, &res[0], len, NULL, NULL);
                            if (!res.empty()) return res;
                        }
                    }
                }
            }
        }
        
        // Fallback: strip .exe
        std::string base = fallbackExe;
        if (base.length() > 4 && base.substr(base.length() - 4) == ".exe") {
            base = base.substr(0, base.length() - 4);
        }
        return base;
    }

    static ActiveProcessInfo getActiveProcess(DWORD ownProcessId) {
        ActiveProcessInfo info;
        HWND hwnd = GetForegroundWindow();
        if (!hwnd) return info;

        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == 0 || pid == ownProcessId) return info;

        info.pid = pid;
        info.hwnd = hwnd;

        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProc) return info;

        wchar_t szPath[MAX_PATH * 2] = {0};
        DWORD pathLen = MAX_PATH * 2;
        if (QueryFullProcessImageNameW(hProc, 0, szPath, &pathLen)) {
            std::wstring fullPath(szPath);
            size_t slash = fullPath.find_last_of(L"\\/");
            std::wstring file = (slash != std::wstring::npos) ? fullPath.substr(slash + 1) : fullPath;
            
            int len = WideCharToMultiByte(CP_UTF8, 0, file.c_str(), -1, NULL, 0, NULL, NULL);
            if (len > 0) {
                std::string exeName(len - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, file.c_str(), -1, &exeName[0], len, NULL, NULL);
                info.exeName = exeName;
                info.appID = exeName;
                info.friendlyName = getFriendlyName(fullPath, exeName);
                info.isBrowser = isBrowserProcess(exeName);
            }
        }
        CloseHandle(hProc);
        return info;
    }
};
