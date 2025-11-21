#include<iostream>
#include<Windows.h>
#include<TlHelp32.h>
#include<Psapi.h>
#include<cctype>
#include<algorithm>
#pragma comment(lib, "Psapi.lib")

void tranform_to_lower(std::wstring& s) {
	std::transform(s.begin(), s.end(), s.begin(), [](char c) { return std::tolower(c); });
}

std::wstring get_process_path(DWORD process_id) {
	std::wstring result = L"<unknown>";
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
	if (!hProcess) {
		return L"<access denied>";
	}
	wchar_t buffer[MAX_PATH];
	DWORD size = MAX_PATH;
	if (QueryFullProcessImageNameW(hProcess, 0, buffer, &size)) {
		result.assign(buffer, size);
	}
	CloseHandle(hProcess);
	return result;
}


SIZE_T get_process_memory_size(DWORD process_id) {
	HANDLE hprocess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, process_id);
	if (!hprocess) {
		std::wcerr << L"PID : " << process_id
			<< L"----------Get memory size failed.-----------\n";
		return (SIZE_T)-1;
	}
	PROCESS_MEMORY_COUNTERS pmc;
	SIZE_T memory_MB = 0;
	if (GetProcessMemoryInfo(hprocess, &pmc, sizeof(pmc))) {
		memory_MB = pmc.WorkingSetSize / (1024 * 1024);
	}
	CloseHandle(hprocess);
	return memory_MB;
}

BOOL kill_process(DWORD process_id) {
	HANDLE hprocess = OpenProcess(PROCESS_TERMINATE, false, process_id);
	if (!hprocess) {
		std::wcerr << L"OpenProcess failed (ACCESS DENIED?). PID=" << process_id << L"\n";
		return false;
	}
	if (!TerminateProcess(hprocess, 1)) {
		std::wcout << L"Terminate process error.ERR: " << GetLastError() << L"\n";
		CloseHandle(hprocess);
		return false;
	}
	CloseHandle(hprocess);
	return true;
}


void list_processes(const std::wstring& filter_name = L"") {
	if (filter_name != L"") {
		std::wcout << L"List processes with filter name contains: " << filter_name << L"\n";
	}
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) {
		std::wcerr << L"CreateToolhelp32Snapshot failed.\n";
		return;
	}
	PROCESSENTRY32W pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32W);
	if (!Process32FirstW(hSnapshot, &pe32)) {
		std::wcerr << L"Process32FirstW failed.\n";
		CloseHandle(hSnapshot);
		return;
	}
	do {
		std::wstring file_name = pe32.szExeFile;
		tranform_to_lower(file_name);
		if (filter_name != L"" && file_name.find(filter_name) == std::wstring::npos) {
			continue;
		}
		SIZE_T mem_MB = get_process_memory_size(pe32.th32ProcessID);
		std::wcout << L"PID: " << pe32.th32ProcessID
			<< L", Name: " << pe32.szExeFile
			<< L", Path: " << get_process_path(pe32.th32ProcessID)
			<< L", Memory(MB): ";
		if (mem_MB != (SIZE_T)-1) { std::wcout << mem_MB; }
		else { std::wcout << L"<unknown>"; }
		std::wcout << L"\n";
	} while (Process32NextW(hSnapshot, &pe32));
}

int wmain() {
	DWORD kill_process_id;
	std::wstring filter_name;
	list_processes();
	std::wcout << L"Enter process id want to kill: ";
	std::wcin >> kill_process_id;
	if (kill_process(kill_process_id)) {
		std::wcout << L"Kill process : " << kill_process_id << L" successfully.";
	}
	std::wcout << L"Enter process name to filter: ";
	std::wcin >> filter_name;
	tranform_to_lower((filter_name));
	list_processes(filter_name);
	return 0;
}