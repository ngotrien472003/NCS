#define _WIN32_WINNT 0x0600
#include "configService.h"
#include<iostream>
#include<Windows.h>

typedef BOOL(__stdcall* PFN_SCANFILE)(
	const wchar_t* filePath,
	BOOL* isDangerous,
	wchar_t* message,
	DWORD messageLen
	);

static const wchar_t* SERVICE_NAME = L"FileScanService";

SERVICE_STATUS 	  g_servicestatus{};
SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
HANDLE g_stopEvent = nullptr;
HANDLE g_workerThread = nullptr;

static ULONGLONG FileTimeToULL(const FILETIME& ft)
{
	ULARGE_INTEGER ul{};
	ul.LowPart = ft.dwLowDateTime;
	ul.HighPart = ft.dwHighDateTime;
	return ul.QuadPart;
}

void ReportServiceStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint) {
	g_servicestatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_servicestatus.dwCurrentState = currentState;
	g_servicestatus.dwWin32ExitCode = win32ExitCode;
	g_servicestatus.dwWaitHint = waitHint;

	if (currentState == SERVICE_START_PENDING) {
		g_servicestatus.dwControlsAccepted = 0;
	}
	else {
		g_servicestatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	}

	if (currentState == SERVICE_RUNNING || currentState == SERVICE_STOPPED) {
		g_servicestatus.dwCheckPoint = 0;
	}
	else {
		g_servicestatus.dwCheckPoint++;
	}
	if (g_statusHandle) {
		SetServiceStatus(g_statusHandle, &g_servicestatus);
	}
}

bool IsSystemIdle(double maxBusyPercent = 40.0)
{
	static bool first = true;
	static ULONGLONG lastIdle = 0;
	static ULONGLONG lastKernel = 0;
	static ULONGLONG lastUser = 0;

	FILETIME ftIdle{}, ftKernel{}, ftUser{};
	if (!GetSystemTimes(&ftIdle, &ftKernel, &ftUser)) {
		return true;
	}

	ULONGLONG idle = FileTimeToULL(ftIdle);
	ULONGLONG kernel = FileTimeToULL(ftKernel);
	ULONGLONG user = FileTimeToULL(ftUser);

	if (first) {
		first = false;
		lastIdle = idle;
		lastKernel = kernel;
		lastUser = user;
		return true;
	}

	ULONGLONG idleDiff = idle - lastIdle;
	ULONGLONG kernelDiff = kernel - lastKernel;
	ULONGLONG userDiff = user - lastUser;
	ULONGLONG total = kernelDiff + userDiff;

	lastIdle = idle;
	lastKernel = kernel;
	lastUser = user;

	if (total == 0) return true;

	double busyPercent =
		(double)(total - idleDiff) * 100.0 / (double)total;

	return busyPercent < maxBusyPercent;
}

DWORD WINAPI ServiceWorkerThread(LPVOID /*lpParam*/) {
	std::wcout << L"Pipe name: " << NAME_PIPE << std::endl;

	wchar_t modulePath[MAX_PATH]{};
	DWORD len = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
	if (len == 0 || len == MAX_PATH) {
		std::wcerr << L"[SERVER] GetModuleFileNameW failed: " << GetLastError() << std::endl;
		return 1;
	}

	wchar_t* lastSlash = wcsrchr(modulePath, L'\\');
	if (lastSlash) {
		*(lastSlash + 1) = L'\0';
	}

	wchar_t dllPath[MAX_PATH]{};
	swprintf_s(dllPath, L"%sscan_engine.dll", modulePath);

	std::wcout << L"[SERVER] Loading engine: " << dllPath << std::endl;

	HMODULE hEngine = LoadLibraryW(dllPath);
	if (!hEngine) {
		std::wcerr << L"[SERVER] LoadLibraryW failed: " << GetLastError() << std::endl;
		return 1;
	}

	PFN_SCANFILE pScanFile =
		reinterpret_cast<PFN_SCANFILE>(
			GetProcAddress(hEngine, "ScanFile")
			);

	if (!pScanFile) {
		std::wcerr << L"[SERVER] GetProcAddress(ScanFile) failed: "
			<< GetLastError() << std::endl;
		FreeLibrary(hEngine);
		return 1;
	}

	std::wcout << L"[SERVER] Engine loaded OK.\n";

	while (true) {
		if (g_stopEvent) {
			DWORD wait = WaitForSingleObject(g_stopEvent, 0);
			if (wait == WAIT_OBJECT_0) {
				std::wcout << L"[SERVER] Stop event signaled, exiting server loop." << std::endl;
				break;
			}
		}
		HANDLE hPipe = CreateNamedPipeW(
			NAME_PIPE,
			PIPE_ACCESS_DUPLEX,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES,
			1024, 1024,
			0, nullptr
		);
		if (hPipe == INVALID_HANDLE_VALUE) {
			std::wcout << L"CreateNamedPipeW failed: " << GetLastError() << std::endl;
			Sleep(1000);
			continue;
		}
		std::wcout << L"Waiting for client connection..." << std::endl;
		BOOL connected = ConnectNamedPipe(hPipe, nullptr) ?
			TRUE :
			(GetLastError() == ERROR_PIPE_CONNECTED);
		if (!connected) {
			std::wcerr << L"ConnectNamedPipe failed: " << GetLastError() << std::endl;
			CloseHandle(hPipe);
			continue;
		}
		std::wcout << L"Client connected!" << std::endl;
		wchar_t buffer[MAX_PATH_CHARS]{};
		DWORD bytesRead = 0;
		BOOL ok = ReadFile(
			hPipe,
			buffer,
			sizeof(buffer) - sizeof(wchar_t),
			&bytesRead,
			nullptr
		);
		if (!ok || bytesRead == 0) {
			std::wcerr << L"ReadFile failed: " << GetLastError() << std::endl;
			CloseHandle(hPipe);
			continue;
		}
		buffer[bytesRead / sizeof(wchar_t)] = L'\0';
		std::wcout << L"Received from client: " << buffer << std::endl;

		wchar_t response[512]{};
		if (!IsSystemIdle()) {
			swprintf_s(response, L"[SERVICE] System is busy, please try again later.");
		}
		else {
			BOOL isDangerous = FALSE;
			wchar_t engineMsg[256]{};

			BOOL scanOk = pScanFile(
				buffer,
				&isDangerous,
				engineMsg,
				_countof(engineMsg)
			);

			if (!scanOk) {
				swprintf_s(response,
					L"[SERVICE] Engine failed to scan file.");
			}
			else {
				swprintf_s(
					response,
					L"[SERVICE] Scan result for \"%s\"\r\n%s\r\nOverall: %s",
					buffer,
					engineMsg,
					isDangerous ? L"DANGEROUS" : L"SAFE"
				);
			}

		}
		DWORD bytesWritten = 0;
		WriteFile(
			hPipe,
			response,
			(DWORD)((wcslen(response) + 1) * sizeof(wchar_t)), // kèm null
			&bytesWritten,
			nullptr
		);

		FlushFileBuffers(hPipe);
		DisconnectNamedPipe(hPipe);
		CloseHandle(hPipe);
		std::wcout << L"[SERVER] Done with this client.\n\n";
	}
	FreeLibrary(hEngine);
	return 0;
}

void WINAPI ServiceCtrlHandler(DWORD ctrlCode) {
	switch (ctrlCode) {
	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN:
		ReportServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
		if (g_stopEvent) {
			SetEvent(g_stopEvent);
		}
		//ReportServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
		break;
	default:
		break;
	}
}






void WINAPI ServiceMain(DWORD /*argc*/, LPWSTR* /*argv*/) {
	g_statusHandle = RegisterServiceCtrlHandlerW(SERVICE_NAME, ServiceCtrlHandler);
	if (!g_statusHandle) {
		return;
	}
	ReportServiceStatus(SERVICE_START_PENDING, NO_ERROR, 3000);
	g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (!g_stopEvent) {
		ReportServiceStatus(SERVICE_STOPPED, GetLastError(), 0);
		return;
	}
	g_workerThread = CreateThread(
		nullptr,
		0,
		ServiceWorkerThread,
		nullptr,
		0,
		nullptr
	);
	if (!g_workerThread) {
		CloseHandle(g_stopEvent);
		ReportServiceStatus(SERVICE_STOPPED, GetLastError(), 0);
		return;
	}
	ReportServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);
	WaitForSingleObject(g_workerThread, INFINITE);
	if (g_workerThread) CloseHandle(g_workerThread);
	if (g_stopEvent)    CloseHandle(g_stopEvent);

	ReportServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
}


int wmain() {
	SERVICE_TABLE_ENTRYW serviceTable[] = {
		{(LPWSTR)SERVICE_NAME,ServiceMain},
		{nullptr, nullptr}
	};
	if (!StartServiceCtrlDispatcherW(serviceTable)) {
		std::wcerr << L"StartServiceCtrlDispatcherW failed: "
			<< GetLastError() << std::endl;
		return 1;
	}
	return 0;
}