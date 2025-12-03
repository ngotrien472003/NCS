#define _wIN32_WINNT 0x0600
#include<iostream>
#include<Windows.h>
#include<string>


const wchar_t* NAME_PIPE = L"\\\\.\\pipe\\FileScanPipe";
DWORD MAX_PATH_CHARS = 260;

int main() {
	std::wcout << "Pipe name: " << NAME_PIPE << std::endl;
	std::wstring path;
	std::wcerr << L"Enter file path to scan: ";
	std::getline(std::wcin, path);

	if (path.size() >= MAX_PATH_CHARS) {
		path.resize(MAX_PATH_CHARS - 1);
	}

	if(!WaitNamedPipeW(NAME_PIPE, 5000)) {
		std::wcerr << L"WaitNamedPipeW failed: " << GetLastError() << std::endl;
		return 1;
	}
	HANDLE hPipe=CreateFileW(NAME_PIPE,
		GENERIC_READ | GENERIC_WRITE,
		0,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr
	);
	if(!hPipe || hPipe==INVALID_HANDLE_VALUE) {
		std::wcerr << L"CreateFileW failed: " << GetLastError() << std::endl;
		return 1;
	}
	DWORD byteToWrite = (DWORD)((path.size() + 1) * sizeof(wchar_t));
	DWORD bytesWritten = 0;
	BOOL ok= WriteFile(
		hPipe,
		path.c_str(),
		byteToWrite,
		&bytesWritten,
		nullptr
	);
	if(!ok) {
		std::wcerr << L"WriteFileEx failed: " << GetLastError() << std::endl;
		CloseHandle(hPipe);
		return 1;
	}
	std::wcout << L"Sent to server: " << path << std::endl;
	std::wcout << L"Bytes written: " << bytesWritten << std::endl;

	wchar_t response[512]{};
	DWORD bytesRead = 0;

	ok = ReadFile(
		hPipe,
		response,
		sizeof(response) - sizeof(wchar_t),
		&bytesRead,
		nullptr
	);

	if (!ok) {
		std::wcerr << L"[CLIENT] ReadFile failed: " << GetLastError() << std::endl;
		CloseHandle(hPipe);
		return 1;
	}
	size_t charsRead = bytesRead / sizeof(wchar_t);
	if (charsRead >= (sizeof(response) / sizeof(wchar_t)))
		charsRead = (sizeof(response) / sizeof(wchar_t)) - 1;
	response[charsRead] = L'\0';

	std::wcout << L"\n===== RESPONSE FROM SERVICE =====\n";
	std::wcout << response << L"\n";
	std::wcout << L"=================================\n";
	CloseHandle(hPipe);

	return 0;
}