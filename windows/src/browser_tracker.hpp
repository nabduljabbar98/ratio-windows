#pragma once
#include <windows.h>
#include <uiautomation.h>
#include <string>
#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

class BrowserTracker {
public:
    static std::string cleanHostname(std::string url) {
        if (url.empty()) return "";

        // Trim leading/trailing whitespace
        while (!url.empty() && (url.front() == ' ' || url.front() == '\t')) url.erase(0, 1);
        while (!url.empty() && (url.back() == ' ' || url.back() == '\t')) url.pop_back();

        // Strip protocol
        size_t proto = url.find("://");
        if (proto != std::string::npos) {
            url = url.substr(proto + 3);
        }

        // Cut off at first slash, question mark, or hash
        size_t slash = url.find_first_of("/?#");
        if (slash != std::string::npos) {
            url = url.substr(0, slash);
        }

        // Cut off port if present
        size_t colon = url.find(':');
        if (colon != std::string::npos) {
            url = url.substr(0, colon);
        }

        // Lowercase
        std::transform(url.begin(), url.end(), url.begin(), ::tolower);

        // Strip "www." prefix
        if (url.rfind("www.", 0) == 0) {
            url = url.substr(4);
        }

        // If it contains whitespace, it's a search term, not a domain
        if (url.find(' ') != std::string::npos || url.find('\t') != std::string::npos) return "";

        // Basic domain validation
        if (url.find('.') == std::string::npos) return "";

        return url;
    }

    static std::string queryActiveBrowserUrl(HWND hwnd) {
        if (!hwnd || !IsWindow(hwnd)) return "";

        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        bool uninit = SUCCEEDED(hr);

        IUIAutomation* pAutomation = nullptr;
        hr = CoCreateInstance(CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER, IID_IUIAutomation, (void**)&pAutomation);
        if (FAILED(hr) || !pAutomation) {
            if (uninit) CoUninitialize();
            return "";
        }

        IUIAutomationElement* pRoot = nullptr;
        hr = pAutomation->ElementFromHandle(hwnd, &pRoot);
        if (FAILED(hr) || !pRoot) {
            pAutomation->Release();
            if (uninit) CoUninitialize();
            return "";
        }

        // Find Edit control (address bar)
        VARIANT varProp;
        varProp.vt = VT_I4;
        varProp.lVal = UIA_EditControlTypeId;

        IUIAutomationCondition* pCondition = nullptr;
        hr = pAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, varProp, &pCondition);
        
        std::string resultUrl;
        if (SUCCEEDED(hr) && pCondition) {
            IUIAutomationElement* pEdit = nullptr;
            hr = pRoot->FindFirst(TreeScope_Descendants, pCondition, &pEdit);
            if (SUCCEEDED(hr) && pEdit) {
                IUIAutomationValuePattern* pValuePattern = nullptr;
                hr = pEdit->GetCurrentPattern(UIA_ValuePatternId, (IUnknown**)&pValuePattern);
                if (SUCCEEDED(hr) && pValuePattern) {
                    BSTR bstrVal = nullptr;
                    if (SUCCEEDED(pValuePattern->get_CurrentValue(&bstrVal)) && bstrVal != nullptr) {
                        int len = WideCharToMultiByte(CP_UTF8, 0, bstrVal, -1, NULL, 0, NULL, NULL);
                        if (len > 0) {
                            std::string raw(len - 1, 0);
                            WideCharToMultiByte(CP_UTF8, 0, bstrVal, -1, &raw[0], len, NULL, NULL);
                            resultUrl = cleanHostname(raw);
                        }
                        SysFreeString(bstrVal);
                    }
                    pValuePattern->Release();
                }
                pEdit->Release();
            }
            pCondition->Release();
        }

        pRoot->Release();
        pAutomation->Release();
        if (uninit) CoUninitialize();

        return resultUrl;
    }

    // Async check helper
    void checkAsync(HWND hwnd, std::function<void(std::string)> onComplete) {
        if (m_busy) return;
        m_busy = true;

        std::thread([this, hwnd, onComplete]() {
            std::string host = queryActiveBrowserUrl(hwnd);
            m_busy = false;
            if (onComplete) onComplete(host);
        }).detach();
    }

    bool isBusy() const { return m_busy; }

private:
    std::atomic<bool> m_busy{false};
};
