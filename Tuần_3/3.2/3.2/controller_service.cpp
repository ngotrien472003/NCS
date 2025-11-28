#include <windows.h>
#include <string>
#include <iostream>

#pragma comment(lib, "Advapi32.lib")

void InstallPerfLoggerService(const std::wstring& binPath)
{
    // binPath: đường dẫn đầy đủ tới PerfLoggerService.exe
    // ví dụ: L"C:\\PerfLoggerService\\PerfLoggerService.exe"

    // Mở Service Control Manager với quyền tạo service
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!hSCM) {
        std::wcerr << L"[!] OpenSCManagerW failed. Error = " << GetLastError() << L"\n";
        return;
    }

    // Tạo service mới
    SC_HANDLE hSvc = CreateServiceW(
        hSCM,
        L"PerfLoggerService",          // service name (internal)
        L"Perf Logger Service",        // display name
        SERVICE_ALL_ACCESS,            // quyền full cho app cài đặt
        SERVICE_WIN32_OWN_PROCESS,     // chạy trong process riêng
        SERVICE_AUTO_START,            // auto start theo hệ thống
        SERVICE_ERROR_NORMAL,
        binPath.c_str(),               // đường dẫn exe
        nullptr, nullptr, nullptr,
        nullptr, nullptr
    );

    if (!hSvc) {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS) {
            std::wcerr << L"[!] Service already exists\n";
        }
        else {
            std::wcerr << L"[!] CreateServiceW failed. Error = " << err << L"\n";
        }
        CloseServiceHandle(hSCM);
        return;
    }

    // Thiết lập auto-restart nếu service bị fail
    SC_ACTION actions[1];
    actions[0].Type = SC_ACTION_RESTART; // hành động: restart
    actions[0].Delay = 60000;             // đợi 60s rồi restart

    SERVICE_FAILURE_ACTIONSW sfa{};
    sfa.dwResetPeriod = 24 * 60 * 60; // sau 1 ngày reset lại count failure
    sfa.cActions = 1;
    sfa.lpsaActions = actions;
    sfa.lpRebootMsg = nullptr;
    sfa.lpCommand = nullptr;

    if (!ChangeServiceConfig2W(hSvc, SERVICE_CONFIG_FAILURE_ACTIONS, &sfa)) {
        std::wcerr << L"[!] ChangeServiceConfig2W(FAILURE_ACTIONS) failed. Error = "
            << GetLastError() << L"\n";
    }

    std::wcout << L"[+] Service PerfLoggerService installed successfully.\n";

    CloseServiceHandle(hSvc);
    CloseServiceHandle(hSCM);
}
