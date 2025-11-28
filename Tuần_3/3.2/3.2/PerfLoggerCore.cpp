#pragma once

#include "LogRotate.h"
#include "PerfLoggerCore.h"
#include <windows.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include<Pdh.h>

#pragma comment(lib, "Pdh.lib")
#pragma comment(lib, "Advapi32.lib")

SERVICE_STATUS_HANDLE g_ServiceStatusHandle = nullptr; 
HANDLE g_StopEvent = nullptr;

void WINAPI ServiceCtrlHandler(DWORD ctrlCode)
{
	
	if (ctrlCode == SERVICE_CONTROL_STOP || ctrlCode == SERVICE_CONTROL_SHUTDOWN)
	{
		if (g_StopEvent) {
			SetEvent(g_StopEvent);
		}
	}
}

std::wstring getLogPath() {
	wchar_t path[MAX_PATH];
	DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
	if (len == 0 || len == MAX_PATH) {
		return L"C:\\PerfLoggerService\\perf_log.txt";
	}
	std::wstring p(path);     
	size_t pos = p.find_last_of(L"\\/");
	if (pos != std::wstring::npos) {
		p = p.substr(0, pos + 1);
	}
	p += L"perf_log.txt";
	return p;
}

std::wstring writeLogLine(const std::wstring& log_path, const std::wstring& line) {
	rotate_log_if_needed(log_path);
	HANDLE hFile = CreateFileW(
		log_path.c_str(),
		FILE_APPEND_DATA,
		FILE_SHARE_READ,
		nullptr,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);
	if (hFile == INVALID_HANDLE_VALUE) return L"";
	std::wstring with_crlf = line + L"\r\n";
	int len = WideCharToMultiByte(
		CP_UTF8,
		0,
		with_crlf.c_str(),
		(int)with_crlf.size(),
		(LPSTR)nullptr,
		0,
		nullptr,
		nullptr
	);
	if (len == 0) {
		CloseHandle(hFile);
		return L"";
	}
	std::string utf8(len, '\0');
	WideCharToMultiByte(
		CP_UTF8,
		0,
		with_crlf.c_str(),
		(int)with_crlf.size(),
		(LPSTR)utf8.data(),
		len,
		nullptr,
		nullptr
	);
	DWORD written = 0;
	WriteFile(hFile, utf8.data(), (DWORD)utf8.size(), &written, nullptr);
	CloseHandle(hFile);
	return L"";
}

bool get_cpu_usage(double& outPercent) {
	static bool initialized = false;
	static ULONGLONG prevIdle = 0, prevKernel = 0, prevUser = 0;
	FILETIME idleTime, kernelTime, userTime;
	if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
		return false;
	}
	ULARGE_INTEGER idle{}, kernel{}, user{};
	idle.LowPart = idleTime.dwLowDateTime;
	idle.HighPart = idleTime.dwHighDateTime;
	kernel.LowPart = kernelTime.dwLowDateTime;
	kernel.HighPart = kernelTime.dwHighDateTime;
	user.LowPart = userTime.dwLowDateTime;
	user.HighPart = userTime.dwHighDateTime;
	if (!initialized) {
		initialized = true;
		prevIdle = idle.QuadPart;
		prevKernel = kernel.QuadPart;
		prevUser = user.QuadPart;
		outPercent = 0.0;
		return true;
	}
	ULONGLONG idleDiff = idle.QuadPart - prevIdle;
	ULONGLONG kernelDiff = kernel.QuadPart - prevKernel;
	ULONGLONG userDiff = user.QuadPart - prevUser;
	prevIdle = idle.QuadPart;
	prevKernel = kernel.QuadPart;
	prevUser = user.QuadPart;
	ULONGLONG total = kernelDiff + userDiff;
	if (total == 0) {
		outPercent = 0.0;
		return true;
	}
	ULONGLONG busy = total - idleDiff;
	outPercent = (double)busy * 100.0 / (double)total;
	return true;
}

bool get_mem_usage(double& out) {
	MEMORYSTATUSEX memInfo{};
	memInfo.dwLength = sizeof(MEMORYSTATUSEX);
	if (!GlobalMemoryStatusEx(&memInfo)) {
		return false;
	}
	out = (double)(memInfo.dwMemoryLoad);
	return true;
}

bool get_disk_usage(double& outPercent) {    
	
	static bool        s_initialized = false;
	static PDH_HQUERY  s_hQuery = nullptr;
	static PDH_HCOUNTER s_hCounter = nullptr;

	PDH_STATUS status;

	
	if (!s_initialized)
	{
	
		status = PdhOpenQueryW(nullptr, 0, &s_hQuery);
		if (status != ERROR_SUCCESS)
		{
			
			return false;
		}

		
		status = PdhAddCounterW(
			s_hQuery,
			L"\\PhysicalDisk(_Total)\\% Disk Time",
			0,
			&s_hCounter
		);
		if (status != ERROR_SUCCESS)
		{
			PdhCloseQuery(s_hQuery);
			s_hQuery = nullptr;
			return false;
		}

		
		status = PdhCollectQueryData(s_hQuery);
		if (status != ERROR_SUCCESS)
		{
			PdhCloseQuery(s_hQuery);
			s_hQuery = nullptr;
			return false;
		}

		s_initialized = true;
		outPercent = 0.0; 
		return true;
	}

	
	status = PdhCollectQueryData(s_hQuery);
	if (status != ERROR_SUCCESS)
	{
		return false;
	}

	PDH_FMT_COUNTERVALUE value{};
	DWORD counterType = 0;

	
	status = PdhGetFormattedCounterValue(
		s_hCounter,
		PDH_FMT_DOUBLE,
		&counterType,
		&value
	);
	if (status != ERROR_SUCCESS)
	{
		return false;
	}

	outPercent = value.doubleValue;

	
	if (outPercent < 0.0)   outPercent = 0.0;
	if (outPercent > 100.0) outPercent = 100.0;

	return true;
}

void log_system_usage() {
	double cpuUsage = 0.0;
	double memUsage = 0.0;
	double diskUsage = 0.0;

	if (!get_cpu_usage(cpuUsage)) {
		std::wcerr << L"Failed to init CPU baseline\n";
		return ;
	}
	if (!get_disk_usage(diskUsage)) {
		std::wcerr << L"Failed to init Disk baseline\n";
		return ;
	}

	std::wcout << std::fixed << std::setprecision(2);

	while (true) {
		Sleep(6000);

		if (!get_cpu_usage(cpuUsage)) {
			std::wcerr << L"Failed to get CPU usage\n";
			continue;
		}
		if (!get_mem_usage(memUsage)) {
			std::wcerr << L"Failed to get Memory usage\n";
			continue;
		}
		if (!get_disk_usage(diskUsage)) {
			std::wcerr << L"Failed to get Disk usage\n";
			continue;
		}

		std::wcout << L"CPU: " << cpuUsage
			<< L"% | RAM: " << memUsage
			<< L"% | DISK: " << diskUsage << L"%\n";

		 std::wstringstream ss;
		 ss << L"CPU=" << cpuUsage << L"% RAM=" << memUsage
		    << L"% DISK=" << diskUsage << L"%";
		 writeLogLine(getLogPath(), ss.str());
	}
}



void WINAPI ServiceMain(DWORD argc, LPWSTR* argv)
{

	SERVICE_STATUS status{};
	status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	status.dwCurrentState = SERVICE_START_PENDING;
	status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	status.dwWin32ExitCode = NO_ERROR;
	status.dwServiceSpecificExitCode = 0;
	status.dwCheckPoint = 0;
	status.dwWaitHint = 0;


	g_ServiceStatusHandle = RegisterServiceCtrlHandlerW(SERVICE_NAME, ServiceCtrlHandler);
	if (!g_ServiceStatusHandle) {
		return; 
	}


	SetServiceStatus(g_ServiceStatusHandle, &status);

	
	g_StopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (!g_StopEvent) {
		status.dwCurrentState = SERVICE_STOPPED;
		status.dwWin32ExitCode = GetLastError();
		SetServiceStatus(g_ServiceStatusHandle, &status);
		return;
	}


	status.dwCurrentState = SERVICE_RUNNING;
	status.dwCheckPoint = 0;
	status.dwWaitHint = 0;
	SetServiceStatus(g_ServiceStatusHandle, &status);

	
	log_system_usage();

	status.dwCurrentState = SERVICE_STOPPED;
	status.dwCheckPoint = 0;
	status.dwWaitHint = 0;
	SetServiceStatus(g_ServiceStatusHandle, &status);

	if (g_StopEvent) {
		CloseHandle(g_StopEvent);
		g_StopEvent = nullptr;
	}
}

int wmain(int argc, wchar_t* argv[])
{
	
	SERVICE_TABLE_ENTRYW serviceTable[] = {
		{ const_cast<LPWSTR>(SERVICE_NAME), ServiceMain },
		{ nullptr, nullptr } 
	};


	if (!StartServiceCtrlDispatcherW(serviceTable)) {
		DWORD err = GetLastError();
		std::wcerr << L"StartServiceCtrlDispatcherW failed: " << err << L"\n";
	}

	return 0;
}