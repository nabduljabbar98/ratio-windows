#pragma once
#include <windows.h>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "storage.hpp"

class Logger {
public:
    static void log(const std::string& message) {
        std::wstring logPath = Storage::getAppDataDirectory() + L"\\ratio.log";
        std::ofstream f(logPath, std::ios::app);
        if (!f.is_open()) return;

        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::tm tm;
        localtime_s(&tm, &in_time_t);

        f << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "." 
          << std::setfill('0') << std::setw(3) << ms.count() << "] "
          << message << "\n";
    }
};
