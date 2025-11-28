#pragma once

#include <string>
#include <Windows.h>

extern const wchar_t* SERVICE_NAME= L"PerfLoggerService";

std::wstring getLogPath();

std::wstring writeLogLine(const std::wstring& log_path,
    const std::wstring& line);

bool get_cpu_usage(double& outPercent);

bool get_mem_usage(double& out);

bool get_disk_usage(double& outPercent);
