#include "LogRotate.h"
#include <windows.h>

const ULONGLONG MAX_LOG_SIZE = 1ULL * 1024 * 1024;     // 1 MB
const ULONGLONG TICKS_PER_DAY = 864000000000ULL;        // 24*60*60*10^7
const ULONGLONG MAX_LOG_AGE = 5ULL * TICKS_PER_DAY;   // 5 ngày

bool should_rotate_log(const std::wstring& log_path) {
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(log_path.c_str(), GetFileExInfoStandard, &fad)) {
        return false;
    }

    ULONGLONG fileSize =
        (static_cast<ULONGLONG>(fad.nFileSizeHigh) << 32) |
        fad.nFileSizeLow;

    FILETIME ftNow{};
    GetSystemTimeAsFileTime(&ftNow);
    ULONGLONG now =
        (static_cast<ULONGLONG>(ftNow.dwHighDateTime) << 32) |
        ftNow.dwLowDateTime;

    ULONGLONG lastWrite =
        (static_cast<ULONGLONG>(fad.ftLastWriteTime.dwHighDateTime) << 32) |
        fad.ftLastWriteTime.dwLowDateTime;

    ULONGLONG age = (now > lastWrite) ? (now - lastWrite) : 0;

    if (fileSize > MAX_LOG_SIZE) return true;
    if (age > MAX_LOG_AGE)       return true;
    return false;
}

void rotate_log_if_needed(const std::wstring& log_path) {
    if (should_rotate_log(log_path)) {
        DeleteFileW(log_path.c_str());
    }
}
