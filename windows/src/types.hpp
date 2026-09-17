#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>

struct AppUsage {
    std::string name;
    double seconds = 0.0;
    double unclassified = 0.0;
    double createSeconds = 0.0;
    double consumeSeconds = 0.0;
    double lastUsed = 0.0;
    bool hasAttribution = false; // true once createSeconds/consumeSeconds have been explicitly set
};

struct Ledger {
    std::string day;
    double consume = 0.0;
    double create = 0.0;
    std::unordered_map<std::string, AppUsage> apps;

    void record(double seconds, const std::string& mode, const std::string& appID = "", const std::string& appName = "") {
        if (seconds <= 0.0 || seconds > 3.0) return;
        
        if (!appID.empty()) {
            auto it = apps.find(appID);
            if (it == apps.end()) {
                AppUsage newEntry;
                newEntry.name = appName.empty() ? appID : appName;
                it = apps.emplace(appID, newEntry).first;
            }
            AppUsage& entry = it->second;
            if (!entry.hasAttribution) {
                double classified = std::max(0.0, entry.seconds - entry.unclassified);
                entry.createSeconds = (mode == "create") ? classified : 0.0;
                entry.consumeSeconds = (mode == "consume") ? classified : 0.0;
                entry.hasAttribution = true;
            }
            if (!appName.empty()) entry.name = appName;
            entry.seconds += seconds;
            auto now = std::chrono::system_clock::now();
            entry.lastUsed = std::chrono::duration<double>(now.time_since_epoch()).count();

            if (mode == "create") {
                entry.createSeconds += seconds;
            } else if (mode == "consume") {
                entry.consumeSeconds += seconds;
            } else if (mode.empty() || mode == "unclassified") {
                entry.unclassified += seconds;
            }
        }

        if (mode == "create") {
            create += seconds;
        } else if (mode == "consume") {
            consume += seconds;
        }
    }

    void classifyPending(const std::string& id, const std::string& mode, const std::string& previousMode = "") {
        if (mode != "create" && mode != "consume" && mode != "neutral") return;
        auto it = apps.find(id);
        if (it == apps.end()) return;

        AppUsage& usage = it->second;
        double classified = std::max(0.0, usage.seconds - usage.unclassified);
        double oldCreate = usage.hasAttribution ? usage.createSeconds : ((previousMode == "create") ? classified : 0.0);
        double oldConsume = usage.hasAttribution ? usage.consumeSeconds : ((previousMode == "consume") ? classified : 0.0);

        create = std::max(0.0, create - oldCreate);
        consume = std::max(0.0, consume - oldConsume);

        usage.createSeconds = (mode == "create") ? usage.seconds : 0.0;
        usage.consumeSeconds = (mode == "consume") ? usage.seconds : 0.0;
        usage.hasAttribution = true;

        create += usage.createSeconds;
        consume += usage.consumeSeconds;
        usage.unclassified = 0.0;
    }
};

struct DaySummary {
    std::string day;
    double create = 0.0;
    double consume = 0.0;
};

struct ResetSnapshot {
    Ledger ledger;
    std::unordered_map<std::string, std::string> rules;
    double activeSeconds = 0.0;
    bool paused = false;
    std::unordered_set<std::string> prompted;
    std::unordered_set<std::string> sessionSites;
    std::string mode;
};

inline std::string dayKey(std::chrono::system_clock::time_point tp = std::chrono::system_clock::now()) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d");
    return ss.str();
}

inline std::string formatDuration(double seconds) {
    int s = std::max(0, static_cast<int>(seconds));
    char buf[64];
    if (s >= 3600) {
        snprintf(buf, sizeof(buf), "%d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
    } else {
        snprintf(buf, sizeof(buf), "%d:%02d", s / 60, s % 60);
    }
    return std::string(buf);
}
