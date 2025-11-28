#include<iostream>
#include<windows.h>
#include<string>

HKEY map_root(const std::wstring& s) {
	if (s == L"HKCU") return HKEY_CURRENT_USER;
	if (s == L"HKLM") return HKEY_LOCAL_MACHINE;
	if (s == L"HKCR") return HKEY_CLASSES_ROOT;
	if (s == L"HKU")  return HKEY_USERS;
	if (s == L"HKCC") return HKEY_CURRENT_CONFIG;
	return nullptr;
}

void parse_dword(const std::wstring& s, DWORD& out) {
	wchar_t* end = nullptr;
	unsigned long val = wcstoul(s.c_str(), &end, 0);
	if (end == s.c_str() || *end != L'\0') {
		std::wcerr << L"[!] Cannot parse DWORD from: " << s << L"\n";
		return;
	}
	out = static_cast<DWORD>(val);
}

void create_registry_key(HKEY& root, std::wstring& subKey, std::wstring& valueName, DWORD type, std::wstring& value) {
	HKEY hKey;
	LSTATUS status = RegCreateKeyExW(
		root,
		subKey.c_str(),
		0,
		nullptr,
		REG_OPTION_NON_VOLATILE,
		KEY_SET_VALUE | KEY_QUERY_VALUE,
		nullptr,
		&hKey,
		nullptr
	);
	if (status != ERROR_SUCCESS) {
		std::wcerr << L"[!] RegCreateKeyExW failed. Error = " << status << L"\n";
		return;
	}

	BYTE* dataPtr = nullptr;
	DWORD cbData = 0;
	DWORD dwVal = 0;
	if (type == REG_SZ) {
		dataPtr = (BYTE*)value.c_str();
		cbData = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
	}
	else if (type == REG_DWORD) {
		parse_dword(value, dwVal);
		dataPtr = (BYTE*)&dwVal;
		cbData = sizeof(DWORD);
	}


	status = RegSetValueExW(
		hKey,
		valueName.c_str(),
		0,
		type,
		dataPtr,
		cbData
	);
	if (status != ERROR_SUCCESS) {
		std::wcerr << L"[!] RegSetKeyValueW failed. Error = " << status << L"\n";
		RegCloseKey(hKey);
		return;
	}
	RegCloseKey(hKey);

}

void delete_registry_value(HKEY& root, std::wstring& subKey, std::wstring& valueName) {
	HKEY hKey;
	LSTATUS status = RegOpenKeyExW(
		root,
		subKey.c_str(),
		0,
		KEY_SET_VALUE,
		&hKey
	);
	if (status != ERROR_SUCCESS) {
		std::wcerr << L"[!] RegOpenKeyExW failed. Error = " << status << L"\n";
		return;
	}
	status = RegDeleteValueW(hKey, valueName.c_str());
	if (status != ERROR_SUCCESS) {
		std::wcerr << L"[!] RegDeleteValueW failed. Error = " << status << L"\n";
		RegCloseKey(hKey);
		return;
	}
	RegCloseKey(hKey);
}

void delete_registry_key(HKEY& root, std::wstring& subKey) {
	LSTATUS status = RegDeleteKeyW(
		root,
		subKey.c_str()
	);
	if (status != ERROR_SUCCESS) {
		std::wcerr << L"[!] RegDeleteKeyW failed. Error = " << status << L"\n";
		return;
	}
}

void delete_registry_tree(HKEY& root, std::wstring& subKey) {
	LSTATUS status = RegDeleteTreeW(
		root,
		subKey.c_str()
	);
	if (status != ERROR_SUCCESS) {
		std::wcerr << L"[!] RegDeleteTreeW failed. Error = " << status << L"\n";
		return;
	}
}

auto enter_params_create_and_update(HKEY& root, std::wstring& subKey, std::wstring& valueName, std::wstring& value, DWORD& type) {
	std::wstring rootStr, typeStr;
	std::wcout << L"Enter root key (HKCU, HKLM, HKCR, HKU, HKCC): ";
	std::getline(std::wcin, rootStr);
	root = map_root(rootStr);
	if (!root) {
		std::wcerr << L"[!] Invalid root key: " << rootStr << L"\n";
		return;
	}
	std::wcout << L"Enter sub key path: ";
	std::getline(std::wcin, subKey);
	std::wcout << L"Enter value name: ";
	std::getline(std::wcin, valueName);
	std::wcout << L"Enter value type (1 for REG_SZ, 4 for REG_DWORD): ";
	std::getline(std::wcin, typeStr);
	std::wcout << L"Enter value data: ";
	std::getline(std::wcin, value);

	if (typeStr == L"1") {
		type = REG_SZ;
	}
	else if (typeStr == L"4") {
		type = REG_DWORD;
	}
	else {
		std::wcerr << L"[!] Invalid type: " << type << L"\n";
		return;
	}

}

int wmain() {
	std::wstring subKey, valueName, value;
	HKEY root;
	DWORD type;

	std::wcout << L"=== Create Registry Key ===\n";
	enter_params_create_and_update(root, subKey, valueName, value, type);
	create_registry_key(root, subKey, valueName, type, value);

	std::wcout << L"=== Update Registry Value ===\n";
	enter_params_create_and_update(root, subKey, valueName, value, type);
	create_registry_key(root, subKey, valueName, type, value);

	std::wcout << L"=== Delete Registry Value ===\n";
	std::wcout << L"Enter root key (HKCU, HKLM, HKCR, HKU, HKCC): ";
	std::wstring rootStr;
	std::getline(std::wcin, rootStr);
	root = map_root(rootStr);
	if (!root) {
		std::wcerr << L"[!] Invalid root key: " << rootStr << L"\n";
		return 1;
	}
	std::wcout << L"Enter sub key path: ";
	std::getline(std::wcin, subKey);
	delete_registry_key(root, subKey);
	return 0;
}