#pragma once
#include <string>

bool InstallService(const std::wstring& service_path);
bool RemoveService();
bool ConfigureServiceAutoRestart(const std::wstring& serviceName);
