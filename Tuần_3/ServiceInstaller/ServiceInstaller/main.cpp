#include "ServiceInstaller.h"

#include <windows.h>
#include <winsvc.h>
#include <iostream>

extern const wchar_t* SERVICE_NAME = L"PerfLoggerService";

bool ConfigureServiceAutoRestart(SC_HANDLE& hService )
{
    /*SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) {
        std::wcerr << L"OpenSCManagerW failed: " << GetLastError() << L"\n";
        return false;
    }

    SC_HANDLE hService = OpenServiceW(hSCM, serviceName.c_str(), SERVICE_CHANGE_CONFIG);
    if (!hService) {
        std::wcerr << L"OpenServiceW failed: " << GetLastError() << L"\n";
        CloseServiceHandle(hSCM);
        return false;
    }*/

    SC_ACTION actions[3]{};

    actions[0].Type = SC_ACTION_RESTART;
    actions[0].Delay = 60 * 1000; // 60s

    actions[1].Type = SC_ACTION_RESTART;
    actions[1].Delay = 60 * 1000;

    actions[2].Type = SC_ACTION_NONE;
    actions[2].Delay = 0;

    SERVICE_FAILURE_ACTIONS sfa{};
    sfa.dwResetPeriod = 24 * 60 * 60; // 1 ngày reset count
    sfa.lpRebootMsg = nullptr;
    sfa.lpCommand = nullptr;
    sfa.cActions = 3;
    sfa.lpsaActions = actions;          

    if (!ChangeServiceConfig2W(
        hService,
        SERVICE_CONFIG_FAILURE_ACTIONS,
        &sfa
    )) {
        std::wcerr << L"ChangeServiceConfig2W(FAILURE_ACTIONS) failed: "
            << GetLastError() << L"\n";
        CloseServiceHandle(hService);
    
        return false;
    }

    SERVICE_FAILURE_ACTIONS_FLAG flag{};
    flag.fFailureActionsOnNonCrashFailures = TRUE;
    if (!ChangeServiceConfig2W(
        hService,
        SERVICE_CONFIG_FAILURE_ACTIONS_FLAG,
        &flag
    )) {
        std::wcerr << L"ChangeServiceConfig2W(FAILURE_ACTIONS_FLAG) failed: "
            << GetLastError() << L"\n";
    }

    CloseServiceHandle(hService);

    return true;
}

bool InstallService(const std::wstring& service_path)
{
    /*wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) {
        std::wcerr << L"GetModuleFileNameW failed: " << GetLastError() << L"\n";
        return false;
    }*/

    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!hSCM) {
        std::wcerr << L"OpenSCManagerW failed: " << GetLastError() << L"\n";
        return false;
    }

    SC_HANDLE hService = CreateServiceW(
        hSCM,
        SERVICE_NAME,
        SERVICE_NAME,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        service_path.c_str(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );

    if (!hService) {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS) {
            std::wcerr << L"Service already exists.\n";
        }
        else {
            std::wcerr << L"CreateServiceW failed: " << err << L"\n";
        }
        CloseServiceHandle(hSCM);
        return false;
    }

    std::wcout << L"Service created successfully.\n";

    if (ConfigureServiceAutoRestart(hService)) {
        std::wcout << L"Auto-restart configured successfully.\n";
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    return true;
}

bool RemoveService()
{
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) {
        std::wcerr << L"OpenSCManagerW failed: " << GetLastError() << L"\n";
        return false;
    }

    SC_HANDLE hService = OpenServiceW(hSCM, SERVICE_NAME, DELETE);
    if (!hService) {
        std::wcerr << L"OpenServiceW failed: " << GetLastError() << L"\n";
        CloseServiceHandle(hSCM);
        return false;
    }

    if (!DeleteService(hService)) {
        std::wcerr << L"DeleteService failed: " << GetLastError() << L"\n";
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCM);
        return false;
    }

    std::wcout << L"Service removed successfully.\n";

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    return true;
}

int main() {
    std::wstring servicePath = L"C:\\Users\\Trien\\source\\repos\\Tuan_3\\Tuan3\\x64\\Debug\\3.2.exe";
	InstallService(servicePath);
    return 0;

}