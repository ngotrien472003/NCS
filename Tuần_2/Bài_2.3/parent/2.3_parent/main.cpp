
#include<iostream>
#include<Windows.h>
#include<string>
#include<fstream>
#include<io.h>
#include<fcntl.h>


int wmain() {
	_setmode(_fileno(stdout), _O_U16TEXT);
	_setmode(_fileno(stderr), _O_U16TEXT);

	std::wcout << L"Enter process ID to scan: ";
	DWORD targetPid = 0;
	std::wcin >> targetPid;
	std::wcout << L"Scanning process ID: " << targetPid << L"\n";

	SECURITY_ATTRIBUTES sa{};
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = nullptr;

	HANDLE hRead = nullptr;
	HANDLE hWrite = nullptr;
	if (!(CreatePipe(&hRead, &hWrite, &sa, 0))) {
		std::wcout << L"Failed to create pipe. Error: " << GetLastError() << L"\n";
		return 1;
	}
	if (!(SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0))) {
		std::wcout << L"Failed to set handle information. Error: " << GetLastError() << L"\n";
		CloseHandle(hRead);
		CloseHandle(hWrite);
		return 1;
	}

	STARTUPINFOW si{};
	si.cb = sizeof(si);
	si.dwFlags |= STARTF_USESTDHANDLES;
	si.hStdOutput = hWrite;
	si.hStdError = hWrite;
	PROCESS_INFORMATION pi{};
	std::wstring commandLine = L"child.exe " + std::to_wstring(targetPid);
	ULONGLONG t0 = GetTickCount64();
	BOOL ok = CreateProcessW(
		nullptr,
		(LPWSTR)commandLine.c_str(),
		nullptr,
		nullptr,
		TRUE,
		0,
		nullptr,
		nullptr,
		&si,
		&pi
	);

	if (!ok) {
		std::wcout << L"CreateProcessW failed. Error: " << GetLastError() << L"\n";
		CloseHandle(hRead);
		CloseHandle(hWrite);
		return 1;
	}
	CloseHandle(hWrite);
	hWrite = nullptr;

	std::wstring childOutput;
	const DWORD BUF_SIZE = 4096;
	BYTE buffer[BUF_SIZE];
	DWORD bytesRead = 0;
	while (true) {
		BOOL success = ReadFile(hRead, buffer, BUF_SIZE, &bytesRead, nullptr);
		if (!success || bytesRead == 0) {
			break;
		}
		size_t wcharCount = bytesRead / sizeof(wchar_t);
		const wchar_t* wbuf = reinterpret_cast<const wchar_t*>(buffer);
		childOutput.append(wbuf, wbuf + wcharCount);
	}
	CloseHandle(hRead);
	WaitForSingleObject(pi.hProcess, INFINITE);
	DWORD exitCode = 0xFFFFFFFF;
	if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
		std::wcout << L"GetExitCodeProcess failed. Error: " << GetLastError() << L"\n";
	}
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	ULONGLONG t1 = GetTickCount64();

	std::wcout << L"=============================================\n";
	std::wcout << L"[PARENT] Scan report\n";
	std::wcout << L"  Target PID: " << targetPid << L"\n";
	std::wcout << L"  Child exit code: " << exitCode << L"\n";
	std::wcout << L"  Child runtime : " << (t1 - t0) << L" ms\n";
	std::wcout << L"--------------- Child STDOUT ---------------\n";
	std::wcout << childOutput << L"\n";
	std::wcout << L"=============================================\n";

	try {
		std::wofstream log(L"edr_log.txt", std::ios::app);
		if (log) {
			log << L"==== PID " << targetPid
				<< L", exit " << exitCode
				<< L", time " << (t1 - t0) << L" ms ====\n";
			log << childOutput << L"\n\n";
		}
	}
	catch (...) {
	}


	return 0;
}