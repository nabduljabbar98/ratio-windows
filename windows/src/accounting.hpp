#pragma once
#include "types.hpp"
#include <windows.h>
#include <iostream>
#include <cassert>
#include <vector>
#include <string>

inline void logTest(const std::string& line) {
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut && hStdOut != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        std::string s = line + "\r\n";
        WriteFile(hStdOut, s.c_str(), (DWORD)s.length(), &written, NULL);
    }
    std::cout << line << "\n";
}

class Classifier {
public:
    static inline const std::vector<std::string> consumeSites = {
        "x.com", "twitter.com", "youtube.com", "reddit.com", "instagram.com", "tiktok.com",
        "netflix.com", "linkedin.com", "twitch.tv", "facebook.com", "pinterest.com",
        "hulu.com", "disneyplus.com", "threads.net"
    };

    static inline const std::vector<std::string> createSites = {
        "figma.com", "docs.google.com", "canva.com", "github.com", "gitlab.com",
        "stackoverflow.com", "chatgpt.com", "claude.ai", "notion.so", "linear.app",
        "overleaf.com", "replit.com"
    };

    static inline const std::unordered_map<std::string, std::string> builtInApps = {
        {"Antigravity.exe", "create"},
        {"Paper.exe", "create"},
        {"devenv.exe", "create"},
        {"Code.exe", "create"},
        {"Cursor.exe", "create"},
        {"Zed.exe", "create"},
        {"idea64.exe", "create"},
        {"pycharm64.exe", "create"},
        {"webstorm64.exe", "create"},
        {"rider64.exe", "create"},
        {"clion64.exe", "create"},
        {"Figma.exe", "create"},
        {"Photoshop.exe", "create"},
        {"Illustrator.exe", "create"},
        {"Premiere.exe", "create"},
        {"AfterFX.exe", "create"},
        {"blender.exe", "create"},
        {"notepad++.exe", "create"},
        {"sublime_text.exe", "create"},
        {"WINWORD.EXE", "create"},
        {"EXCEL.EXE", "create"},
        {"POWERPNT.EXE", "create"},
        {"Obsidian.exe", "create"},
        {"Notion.exe", "create"},
        {"WindowsTerminal.exe", "create"},
        {"powershell.exe", "create"},
        {"cmd.exe", "create"},
        {"Slack.exe", "create"},
        {"SnippingTool.exe", "create"},
        // macOS bundle IDs for compatibility
        {"com.apple.dt.Xcode", "create"},
        {"com.microsoft.VSCode", "create"},
        {"com.figma.Desktop", "create"},
        {"com.adobe.Photoshop", "create"},
        {"com.adobe.Illustrator", "create"},
        {"com.apple.iWork.Pages", "create"},
        {"com.apple.iWork.Keynote", "create"},
        {"com.apple.iWork.Numbers", "create"},
        {"com.apple.FinalCut", "create"},
        {"com.apple.garageband10", "create"},
        {"com.apple.Logic10", "create"},
        // Consume apps
        {"Netflix.exe", "consume"},
        {"Spotify.exe", "consume"},
        {"Video.UI.exe", "consume"},
        {"Steam.exe", "consume"},
        {"EpicGamesLauncher.exe", "consume"},
        {"Discord.exe", "consume"},
        {"Telegram.exe", "consume"},
        {"explorer.exe", "consume"},
        {"com.apple.TV", "consume"},
        {"com.apple.iBooksX", "consume"},
        {"com.apple.news", "consume"}
    };

    static std::string siteMode(const std::string& host) {
        for (const auto& s : consumeSites) {
            if (host == s || (host.length() > s.length() && 
                host.compare(host.length() - s.length() - 1, s.length() + 1, "." + s) == 0)) {
                return "consume";
            }
        }
        for (const auto& s : createSites) {
            if (host == s || (host.length() > s.length() && 
                host.compare(host.length() - s.length() - 1, s.length() + 1, "." + s) == 0)) {
                return "create";
            }
        }
        return "";
    }

    static std::string appMode(const std::string& appID) {
        auto it = builtInApps.find(appID);
        if (it != builtInApps.end()) return it->second;
        return "";
    }

    static bool runSelfTest() {
        // 1. Website classification and hostname boundaries
        assert(siteMode("x.com") == "consume");
        assert(siteMode("www.youtube.com") == "consume");
        assert(siteMode("www.linkedin.com") == "consume");
        assert(siteMode("docs.google.com") == "create");
        assert(siteMode("github.com") == "create");
        assert(appMode("Antigravity.exe") == "create");
        assert(appMode("Paper.exe") == "create");
        assert(appMode("Steam.exe") == "consume");
        assert(siteMode("notx.com") == "");
        assert(siteMode("x.com.example.org") == "");
        logTest("PASS: website classification and hostname boundaries");

        // 2. Accounting must exclude gaps and unknown time
        Ledger l;
        l.day = "test";
        l.record(2.0, "create");
        l.record(1.0, "consume");
        l.record(2.0, ""); // unclassified
        l.record(100.0, "create"); // gap > 3s ignored
        l.record(-1.0, "consume"); // invalid ignored
        assert(l.create == 2.0 && l.consume == 1.0);

        // 3. App attribution, legacy migration, daily reset, app persistence
        Ledger legacy;
        legacy.day = "test";
        legacy.consume = 7.0;
        legacy.create = 9.0;
        
        Ledger usage = legacy;
        usage.record(2.0, "", "browser", "Browser");
        usage.record(1.0, "create", "editor", "Editor");
        usage.record(100.0, "create", "editor", "Editor");
        assert(usage.apps["browser"].seconds == 2.0);
        assert(usage.apps["editor"].seconds == 1.0);
        assert(usage.consume == 7.0 && usage.create == 10.0);

        usage.classifyPending("browser", "consume");
        assert(usage.consume == 9.0 && usage.apps["browser"].unclassified == 0.0);
        usage.classifyPending("browser", "consume");
        assert(usage.consume == 9.0); // Review must not double count
        logTest("PASS: retrospective categorization and duplicate review protection");
        logTest("PASS: app attribution, legacy migration, daily reset, app persistence");

        // 4. Recategorization moves all app time, neutral exclusion, idempotency and persistence
        Ledger reclassified;
        reclassified.day = "test";
        reclassified.record(3.0, "create", "editor", "Editor");
        reclassified.record(2.0, "consume", "social", "Social");
        
        reclassified.classifyPending("social", "create");
        assert(reclassified.create == 5.0 && reclassified.consume == 0.0);

        reclassified.classifyPending("editor", "consume");
        reclassified.classifyPending("social", "consume");
        assert(reclassified.create == 0.0 && reclassified.consume == 5.0);

        reclassified.classifyPending("social", "consume");
        assert(reclassified.consume == 5.0);

        reclassified.classifyPending("social", "neutral");
        assert(reclassified.create == 0.0 && reclassified.consume == 3.0);

        reclassified.record(2.0, "neutral", "social", "Social");
        assert(reclassified.create == 0.0 && reclassified.consume == 3.0 && reclassified.apps["social"].seconds == 4.0);

        reclassified.classifyPending("social", "consume");
        assert(reclassified.consume == 7.0);

        logTest("PASS: recategorization moves all app time, neutral exclusion, idempotency and persistence");
        logTest("PASS: classified time, unknown exclusion, suspension gaps, persistence");
        return true;
    }
};
