#pragma once
#include "types.hpp"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

class Storage {
public:
    static std::wstring getAppDataDirectory() {
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path))) {
            std::wstring dir = std::wstring(path) + L"\\Ratio";
            CoTaskMemFree(path);
            CreateDirectoryW(dir.c_str(), NULL);
            return dir;
        }
        return L".\\";
    }

    static std::wstring getDataFilePath() {
        return getAppDataDirectory() + L"\\ratio_data.json";
    }

    static std::string escapeJson(const std::string& s) {
        std::ostringstream o;
        for (char c : s) {
            if (c == '"') o << "\\\"";
            else if (c == '\\') o << "\\\\";
            else if (c == '\b') o << "\\b";
            else if (c == '\f') o << "\\f";
            else if (c == '\n') o << "\\n";
            else if (c == '\r') o << "\\r";
            else if (c == '\t') o << "\\t";
            else if ('\x00' <= c && c <= '\x1f') {
                o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
            } else {
                o << c;
            }
        }
        return o.str();
    }

    static void save(const Ledger& ledger, const std::vector<DaySummary>& history, 
                     const std::unordered_map<std::string, std::string>& rules,
                     bool lightMode, bool paused, double telemetrySeconds, bool telemetryEnabled,
                     const std::string& installID) {
        std::wstring path = getDataFilePath();
        std::ofstream f(path);
        if (!f.is_open()) return;

        f << "{\n";
        f << "  \"lightMode\": " << (lightMode ? "true" : "false") << ",\n";
        f << "  \"paused\": " << (paused ? "true" : "false") << ",\n";
        f << "  \"anonymousTotalsEnabled\": " << (telemetryEnabled ? "true" : "false") << ",\n";
        f << "  \"anonymousTrackedSeconds\": " << telemetrySeconds << ",\n";
        f << "  \"anonymousInstallID\": \"" << escapeJson(installID) << "\",\n";

        // Rules
        f << "  \"rules\": {\n";
        size_t rIndex = 0;
        for (const auto& [k, v] : rules) {
            f << "    \"" << escapeJson(k) << "\": \"" << escapeJson(v) << "\"" 
              << (++rIndex < rules.size() ? ",\n" : "\n");
        }
        f << "  },\n";

        // Ledger
        f << "  \"ledger\": {\n";
        f << "    \"day\": \"" << escapeJson(ledger.day) << "\",\n";
        f << "    \"create\": " << ledger.create << ",\n";
        f << "    \"consume\": " << ledger.consume << ",\n";
        f << "    \"apps\": {\n";
        size_t aIndex = 0;
        for (const auto& [k, v] : ledger.apps) {
            f << "      \"" << escapeJson(k) << "\": {\n";
            f << "        \"name\": \"" << escapeJson(v.name) << "\",\n";
            f << "        \"seconds\": " << v.seconds << ",\n";
            f << "        \"unclassified\": " << v.unclassified << ",\n";
            f << "        \"createSeconds\": " << v.createSeconds << ",\n";
            f << "        \"consumeSeconds\": " << v.consumeSeconds << ",\n";
            f << "        \"lastUsed\": " << v.lastUsed << ",\n";
            f << "        \"hasAttribution\": " << (v.hasAttribution ? "true" : "false") << "\n";
            f << "      }" << (++aIndex < ledger.apps.size() ? ",\n" : "\n");
        }
        f << "    }\n";
        f << "  },\n";

        // History
        f << "  \"history\": [\n";
        for (size_t i = 0; i < history.size(); ++i) {
            f << "    {\n";
            f << "      \"day\": \"" << escapeJson(history[i].day) << "\",\n";
            f << "      \"create\": " << history[i].create << ",\n";
            f << "      \"consume\": " << history[i].consume << "\n";
            f << "    }" << (i + 1 < history.size() ? ",\n" : "\n");
        }
        f << "  ]\n";
        f << "}\n";
    }

    // Lightweight parsing helper
    static std::string extractString(const std::string& src, const std::string& key) {
        size_t pos = src.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        pos = src.find(':', pos);
        if (pos == std::string::npos) return "";
        size_t start = src.find('"', pos);
        if (start == std::string::npos) return "";
        size_t end = src.find('"', start + 1);
        while (end != std::string::npos && src[end - 1] == '\\') {
            end = src.find('"', end + 1);
        }
        if (end == std::string::npos) return "";
        return src.substr(start + 1, end - start - 1);
    }

    static double extractDouble(const std::string& src, const std::string& key, double def = 0.0) {
        size_t pos = src.find("\"" + key + "\"");
        if (pos == std::string::npos) return def;
        pos = src.find(':', pos);
        if (pos == std::string::npos) return def;
        while (pos < src.length() && (src[pos] == ':' || src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n' || src[pos] == '\r')) pos++;
        size_t end = src.find_first_of(",}\n\r", pos);
        if (end == std::string::npos) end = src.length();
        try {
            return std::stod(src.substr(pos, end - pos));
        } catch (...) {
            return def;
        }
    }

    static bool extractBool(const std::string& src, const std::string& key, bool def = false) {
        size_t pos = src.find("\"" + key + "\"");
        if (pos == std::string::npos) return def;
        pos = src.find(':', pos);
        if (pos == std::string::npos) return def;
        if (src.find("true", pos) < src.find_first_of(",}\n", pos)) return true;
        if (src.find("false", pos) < src.find_first_of(",}\n", pos)) return false;
        return def;
    }

    static bool load(Ledger& ledger, std::vector<DaySummary>& history,
                     std::unordered_map<std::string, std::string>& rules,
                     bool& lightMode, bool& paused, double& telemetrySeconds, bool& telemetryEnabled,
                     std::string& installID) {
        std::wstring path = getDataFilePath();
        std::ifstream f(path);
        if (!f.is_open()) return false;

        std::stringstream buffer;
        buffer << f.rdbuf();
        std::string s = buffer.str();
        if (s.empty()) return false;

        lightMode = extractBool(s, "lightMode", false);
        paused = extractBool(s, "paused", false);
        telemetryEnabled = extractBool(s, "anonymousTotalsEnabled", true);
        telemetrySeconds = extractDouble(s, "anonymousTrackedSeconds", 0.0);
        installID = extractString(s, "anonymousInstallID");

        // Parse rules section
        size_t rPos = s.find("\"rules\"");
        if (rPos != std::string::npos) {
            size_t rStart = s.find('{', rPos);
            size_t rEnd = s.find('}', rStart);
            if (rStart != std::string::npos && rEnd != std::string::npos) {
                std::string rSection = s.substr(rStart, rEnd - rStart + 1);
                size_t p = 0;
                while ((p = rSection.find('"', p)) != std::string::npos) {
                    size_t kEnd = rSection.find('"', p + 1);
                    if (kEnd == std::string::npos) break;
                    std::string key = rSection.substr(p + 1, kEnd - p - 1);
                    size_t colon = rSection.find(':', kEnd);
                    if (colon == std::string::npos) break;
                    size_t vStart = rSection.find('"', colon);
                    if (vStart == std::string::npos || vStart > rSection.find_first_of(",}", colon)) {
                        p = kEnd + 1;
                        continue;
                    }
                    size_t vEnd = rSection.find('"', vStart + 1);
                    if (vEnd == std::string::npos) break;
                    std::string val = rSection.substr(vStart + 1, vEnd - vStart - 1);
                    if (val != "neutral") rules[key] = val;
                    p = vEnd + 1;
                }
            }
        }

        // Parse ledger
        size_t lPos = s.find("\"ledger\"");
        if (lPos != std::string::npos) {
            size_t lStart = s.find('{', lPos);
            if (lStart != std::string::npos) {
                // Find matching close brace for ledger or parse until history
                size_t histPos = s.find("\"history\"");
                std::string lSection = (histPos != std::string::npos) ? s.substr(lStart, histPos - lStart) : s.substr(lStart);
                
                std::string day = extractString(lSection, "day");
                if (!day.empty()) ledger.day = day;
                ledger.create = extractDouble(lSection, "create", 0.0);
                ledger.consume = extractDouble(lSection, "consume", 0.0);

                // Parse apps
                size_t aPos = lSection.find("\"apps\"");
                if (aPos != std::string::npos) {
                    size_t aStart = lSection.find('{', aPos);
                    size_t aEnd = lSection.rfind('}');
                    if (aStart != std::string::npos && aEnd != std::string::npos && aEnd > aStart) {
                        std::string aSection = lSection.substr(aStart, aEnd - aStart + 1);
                        size_t p = 1;
                        while ((p = aSection.find('"', p)) != std::string::npos) {
                            size_t idEnd = aSection.find('"', p + 1);
                            if (idEnd == std::string::npos) break;
                            std::string id = aSection.substr(p + 1, idEnd - p - 1);
                            size_t objStart = aSection.find('{', idEnd);
                            if (objStart == std::string::npos) break;
                            size_t objEnd = aSection.find('}', objStart);
                            if (objEnd == std::string::npos) break;

                            std::string objStr = aSection.substr(objStart, objEnd - objStart + 1);
                            AppUsage u;
                            u.name = extractString(objStr, "name");
                            if (u.name.empty()) u.name = id;
                            u.seconds = extractDouble(objStr, "seconds", 0.0);
                            u.unclassified = extractDouble(objStr, "unclassified", 0.0);
                            u.createSeconds = extractDouble(objStr, "createSeconds", 0.0);
                            u.consumeSeconds = extractDouble(objStr, "consumeSeconds", 0.0);
                            u.lastUsed = extractDouble(objStr, "lastUsed", 0.0);
                            u.hasAttribution = extractBool(objStr, "hasAttribution", true);

                            ledger.apps[id] = u;
                            p = objEnd + 1;
                        }
                    }
                }
            }
        }

        // Parse history
        size_t hPos = s.find("\"history\"");
        if (hPos != std::string::npos) {
            size_t hStart = s.find('[', hPos);
            size_t hEnd = s.find(']', hStart);
            if (hStart != std::string::npos && hEnd != std::string::npos) {
                std::string hSection = s.substr(hStart, hEnd - hStart + 1);
                size_t p = 0;
                while ((p = hSection.find('{', p)) != std::string::npos) {
                    size_t objEnd = hSection.find('}', p);
                    if (objEnd == std::string::npos) break;
                    std::string item = hSection.substr(p, objEnd - p + 1);
                    DaySummary ds;
                    ds.day = extractString(item, "day");
                    ds.create = extractDouble(item, "create", 0.0);
                    ds.consume = extractDouble(item, "consume", 0.0);
                    if (!ds.day.empty()) history.push_back(ds);
                    p = objEnd + 1;
                }
            }
        }

        return true;
    }
};
