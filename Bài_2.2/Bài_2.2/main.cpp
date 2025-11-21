
#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <unordered_map>
#include <cstdint> 
#include <conio.h> 
#include<string>
#pragma comment(lib, "Psapi.lib")

const uint64_t TICKS_PER_SECOND = 10000000;



struct ThreadInfo {
	uint64_t cpuTime_1, cpuTime_2, wallTime_1, wallTime_2;
	std::wstring status;
	float cpu_usage;
};

uint64_t convert_filetime_to_uint64(FILETIME& ft) {
	uint64_t time = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
	return time;
}

void get_cpu_usage_thread(std::unordered_map<DWORD, ThreadInfo>& threads, int num_cpu) {
	for (auto& t : threads) {
		SIZE_T cpuTime = t.second.cpuTime_2 - t.second.cpuTime_1;
		SIZE_T wallTime = t.second.wallTime_2 - t.second.wallTime_1;
		float cpu_usage = cpuTime * 100.0f / (wallTime * num_cpu);
		t.second.cpu_usage = cpu_usage;
	}
}
auto get_list_threads_info(DWORD& process_id, std::unordered_map<DWORD, ThreadInfo>& threads, BOOL isFirstCheck = true) {
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) {
		std::wcerr << L"CreateToolhelp32Snapshot failed.\n";
		return;
	}
	THREADENTRY32 te32;
	te32.dwSize = sizeof(THREADENTRY32);
	if (!Thread32First(hSnapshot, &te32)) {
		std::wcerr << L"Thread32First failed.\n";
		CloseHandle(hSnapshot);
		return;
	}
	do {
		if (te32.th32OwnerProcessID != process_id) continue;
		if (threads.find(te32.th32ThreadID) == threads.end() and isFirstCheck == false) continue;
		//std::wcout << L"Thread ID: " << te32.th32ThreadID << L", Process ID: " << te32.th32OwnerProcessID << L"\n";
		HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID);
		if (!hThread) {
			std::wcerr << L"open thread failed\n";
			continue;
		}
		FILETIME creationTime, exitTime, kernelTime, userTime;
		if (!GetThreadTimes(hThread, &creationTime, &exitTime, &kernelTime, &userTime)) {
			std::wcerr << L"GetThreadTimes failed for Thread ID: " << te32.th32ThreadID << L"\n";
			CloseHandle(hThread);
		}
		else {
			ThreadInfo ti;
			FILETIME now;
			GetSystemTimeAsFileTime(&now);
			uint64_t cpuTime = (convert_filetime_to_uint64(kernelTime)) + convert_filetime_to_uint64(userTime);
			uint64_t wallTime = convert_filetime_to_uint64(now);
			if (isFirstCheck) {
				ti.cpuTime_1 = cpuTime;
				ti.wallTime_1 = wallTime;
				threads[te32.th32ThreadID] = ti;
			}
			else {
				threads[te32.th32ThreadID].cpuTime_2 = cpuTime;
				threads[te32.th32ThreadID].wallTime_2 = wallTime;
			}

			CloseHandle(hThread);
		}
	} while (Thread32Next(hSnapshot, &te32));
	CloseHandle(hSnapshot);
}



auto list_threads_in_process(DWORD process_id, int num_cpu) {
	std::unordered_map<DWORD, ThreadInfo> threads;
	get_list_threads_info(process_id, threads);
	Sleep(1000);
	get_list_threads_info(process_id, threads, false);
	get_cpu_usage_thread(threads, num_cpu);
	float total_cpu = 0.0f;
	for (auto& t : threads) {
		total_cpu += t.second.cpu_usage;
		std::wcout << L"Thread ID: " << t.first << L", CPU Usage: " << t.second.cpu_usage << L"%\n";
	}
	std::wcout << L"-------------------------------------------\n";
	std::wcout << L"Total CPU Usage: " << total_cpu << L"%\n";
}


int wmain() {
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	DWORD process_id;
	std::wcout << L"Enter Process ID: ";
	std::wcin >> process_id;

	while (true) {
		system("cls");
		std::wcout << L"Threads CPU Usage for PID = " << process_id << L"\n";
		std::wcout << L"-------------------------------------------\n";
		list_threads_in_process(process_id, si.dwNumberOfProcessors);
		/*std::wcout << L"Press Q to quit or Enter to refresh...\n";
		std::wcin >> std::ws;
		std::wstring cmd;
		std::getline(std::wcin, cmd);
		if (cmd == L"Q" || cmd == L"q") {
			break;
		}*/
		Sleep(1000);

	}
	return 0;
}
