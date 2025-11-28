#include <windows.h>
#include<winsvc.h>
#include <iostream>
#include<vector>
#include<string>
#include<fstream>
#include <codecvt>
#include <locale>

bool start_service(const std::wstring& name) {
	SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
	if (!hSCM) {
		std::wcout << L"OpenSCManagerW failed\n";
		return false;
	}

	SC_HANDLE hSvc = OpenServiceW(
		hSCM,
		name.c_str(),
		SERVICE_START | SERVICE_QUERY_STATUS
	);
	if (!hSvc) {
		std::wcout << L"OpenServiceW failed\n";
		CloseServiceHandle(hSCM);
		return false;
	}

	if (!StartServiceW(hSvc, 0, nullptr)) {
		DWORD err = GetLastError();
		if (err == ERROR_SERVICE_ALREADY_RUNNING) {
			std::wcout << L"Service is already running.\n";
		}
		else {
			std::wcout << L"StartServiceW failed\n";
			CloseServiceHandle(hSvc);
			CloseServiceHandle(hSCM);
			return false;
		}
	}
	                                
	SERVICE_STATUS_PROCESS ssp{};
	DWORD bytesNeeded = 0;
	while (true) {
		if (!QueryServiceStatusEx(
			hSvc,
			SC_STATUS_PROCESS_INFO,
			reinterpret_cast<LPBYTE>(&ssp),
			sizeof(ssp),
			&bytesNeeded
		)) {
			std::wcout << L"QueryServiceStatusEx failed\n";
			break;
		}

		if (ssp.dwCurrentState == SERVICE_RUNNING) {
			std::wcout << L"Service started successfully.\n";
			break;
		}
		else if (ssp.dwCurrentState == SERVICE_STOPPED) {
			std::wcout << L"Service stopped with exit code "
				<< ssp.dwWin32ExitCode << L"\n";
			break;
		}

		Sleep(500);
	}

	CloseServiceHandle(hSvc);
	CloseServiceHandle(hSCM);
	return true;
}


bool stop_service(const std::wstring& name) {
	SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
	if (!hSCM) {
		std::wcout << "OpenSCManagerW failed\n";	
		return false;
	}

	SC_HANDLE hSvc = OpenServiceW(
		hSCM,
		name.c_str(),
		SERVICE_STOP | SERVICE_QUERY_STATUS
	);
	if (!hSvc) {
		std::wcout << "OpenServiceW failed\n";
		CloseServiceHandle(hSCM);
		return false;
	}

	SERVICE_STATUS_PROCESS ssp{};
	DWORD bytesNeeded = 0;
	if (!ControlService(hSvc, SERVICE_CONTROL_STOP,
		reinterpret_cast<LPSERVICE_STATUS>(&ssp))
		) {
		std::wcout << "ControlService(SERVICE_CONTROL_STOP) failed\n";
		CloseServiceHandle(hSvc);
		CloseServiceHandle(hSCM);
		return false;
	}

	while (ssp.dwCurrentState != SERVICE_STOPPED) {  
		Sleep(500);
		if (!QueryServiceStatusEx(
			hSvc,
			SC_STATUS_PROCESS_INFO,
			reinterpret_cast<LPBYTE>(&ssp),
			sizeof(ssp),
			&bytesNeeded
		)) {
			std::wcout << "QueryServiceStatusEx failed\n";
			break;
		}
	}

	std::wcout << L"Service stopped.\n";

	CloseServiceHandle(hSvc);
	CloseServiceHandle(hSCM);
	return true;
}


void list_services() {
	std::wofstream logFile("services_log.txt");
	if (!logFile) {
		std::wcerr << L"[!] Cannot open services_log.txt for writing\n";
		return;
	}
	std::locale utf8_locale(
		logFile.getloc(),
		new std::codecvt_utf8_utf16<wchar_t>
	);
	logFile.imbue(utf8_locale);
	SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
	if (!hSCM) {
		std::wcerr << L"[!] OpenSCManagerW failed. Error = " << GetLastError() << L"\n";
		return;
	}
	DWORD bytesNeeded = 0;
	DWORD servicesReturned = 0;
	DWORD resumeHandle = 0;
	EnumServicesStatusExW(
		hSCM,
		SC_ENUM_PROCESS_INFO,
		SERVICE_WIN32, 
		SERVICE_STATE_ALL,
		nullptr,
		0,
		&bytesNeeded,
		&servicesReturned,
		&resumeHandle,
		nullptr
		);
	DWORD bufSize = bytesNeeded;
	std::vector<BYTE> buffer(bufSize);
	BOOL result = EnumServicesStatusExW(
		hSCM,
		SC_ENUM_PROCESS_INFO,
		SERVICE_WIN32,
		SERVICE_STATE_ALL,
		buffer.data(),
		bufSize,
		&bytesNeeded,
		&servicesReturned,
		&resumeHandle,
		nullptr
	);
	if (!result) {
		std::wcerr << L"[!] EnumServicesStatusExW failed. Error = " << GetLastError() << L"\n";
		CloseServiceHandle(hSCM);
		return;
	}

	LPENUM_SERVICE_STATUS_PROCESSW services = reinterpret_cast<LPENUM_SERVICE_STATUS_PROCESSW>(buffer.data());
	std::wcout << L"Found " << servicesReturned << L" services:\n\n";

	for (DWORD i = 0; i < servicesReturned; ++i) {
		if (!logFile) {
			std::wcerr << L"[!] logFile is in error state at index " << GetLastError() << L"\n";
			break;
		}
		auto& s = services[i];
		logFile << L"ServiceName : " << s.lpServiceName << L"\n";
		logFile << L"DisplayName : " << s.lpDisplayName << L"\n";
		logFile << L"State       : ";

		switch (s.ServiceStatusProcess.dwCurrentState) {
		case SERVICE_STOPPED: logFile << L"STOPPED"; break;
		case SERVICE_START_PENDING: logFile << L"START_PENDING"; break;
		case SERVICE_STOP_PENDING: logFile << L"STOP_PENDING"; break;
		case SERVICE_RUNNING: logFile << L"RUNNING"; break;
		case SERVICE_CONTINUE_PENDING: logFile << L"CONTINUE_PENDING"; break;
		case SERVICE_PAUSE_PENDING: logFile << L"PAUSE_PENDING"; break;
		case SERVICE_PAUSED: logFile << L"PAUSED"; break;
		default: logFile << L"UNKNOWN"; break;
		}
		logFile << L"\n\n";
	}

	CloseServiceHandle(hSCM);
}



int main(){
	list_services();
	std::wstring serviceName_start;
	std::wstring serviceName_stop;
	std::wcout << L"Enter service name to start: ";
	std::getline(std::wcin, serviceName_start);
	start_service(serviceName_start);
	std::wcout << L"Enter service name to stop: ";
	std::getline(std::wcin, serviceName_stop);
	stop_service(serviceName_stop);
	return 0;
}

