#pragma once
#include <string>

bool should_rotate_log(const std::wstring& log_path);
void rotate_log_if_needed(const std::wstring& log_path);
