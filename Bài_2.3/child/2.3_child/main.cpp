#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <io.h>
#include <fcntl.h>
#include <wchar.h>

#pragma comment(lib, "Psapi.lib")

std::wstring json_escape(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());
    for (wchar_t c : s) {
        switch (c) {
        case L'\"': out += L"\\\""; break;
        case L'\\': out += L"\\\\"; break;
        case L'\n': out += L"\\n";  break;
        case L'\r': out += L"\\r";  break;
        case L'\t': out += L"\\t";  break;
        default:
            if (c < 32) {
                wchar_t buf[7];
                swprintf(buf, 7, L"\\u%04X", (unsigned)c);
                out += buf;
            }
            else {
                out += c;
            }
        }
    }
    return out;
}

std::wstring filetime_to_string(const FILETIME& ftUtc) {
    FILETIME localFt{};
    SYSTEMTIME st{};
    if (!FileTimeToLocalFileTime(&ftUtc, &localFt) ||
        !FileTimeToSystemTime(&localFt, &st)) {
        return L"";
    }
    wchar_t buf[64];
    swprintf(buf, 64, L"%04u-%02u-%02u %02u:%02u:%02u",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);
    return buf;
}

bool get_process_image_path(DWORD pid, std::wstring& outPath) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;

    std::wstring buf;
    buf.resize(32768);

    DWORD size = (DWORD)buf.size();
    if (!QueryFullProcessImageNameW(h, 0, (LPWSTR)buf.data(), &size)) {
        CloseHandle(h);
        return false;
    }

    buf.resize(size);
    outPath = buf;
    CloseHandle(h);
    return true;
}

bool get_process_memory_info(DWORD pid, SIZE_T& workingSet) {
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!h) return false;

    PROCESS_MEMORY_COUNTERS pmc{};
    if (!GetProcessMemoryInfo(h, &pmc, sizeof(pmc))) {
        CloseHandle(h);
        return false;
    }

    workingSet = pmc.WorkingSetSize;
    CloseHandle(h);
    return true;
}

bool get_process_times_simple(DWORD pid,
    FILETIME& createTime,
    FILETIME& exitTime,
    FILETIME& kernelTime,
    FILETIME& userTime) {
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!h) return false;

    BOOL ok = GetProcessTimes(h, &createTime, &exitTime, &kernelTime, &userTime);
    CloseHandle(h);
    return ok == TRUE;
}

bool get_parent_pid(DWORD pid, DWORD& ppid) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    if (!Process32FirstW(hSnap, &pe)) {
        CloseHandle(hSnap);
        return false;
    }

    do {
        if (pe.th32ProcessID == pid) {
            ppid = pe.th32ParentProcessID;
            CloseHandle(hSnap);
            return true;
        }
    } while (Process32NextW(hSnap, &pe));

    CloseHandle(hSnap);
    return false;
}

std::vector<std::wstring> list_modules(DWORD pid) {
    std::vector<std::wstring> modules;

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (hSnap == INVALID_HANDLE_VALUE) {
        return modules;
    }

    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);

    if (!Module32FirstW(hSnap, &me)) {
        CloseHandle(hSnap);
        return modules;
    }

    do {
        modules.push_back(me.szExePath);
    } while (Module32NextW(hSnap, &me));

    CloseHandle(hSnap);
    return modules;
}

void print_scan_result_json(DWORD pid) {
    std::wstring imagePath;
    bool hasPath = get_process_image_path(pid, imagePath);

    DWORD ppid = 0;
    bool hasParent = get_parent_pid(pid, ppid);

    SIZE_T workingSet = 0;
    bool hasMem = get_process_memory_info(pid, workingSet);

    FILETIME ftCreate{}, ftExit{}, ftKernel{}, ftUser{};
    bool hasTimes = get_process_times_simple(pid, ftCreate, ftExit, ftKernel, ftUser);

    std::vector<std::wstring> modules = list_modules(pid);

    std::wstring createStr = hasTimes ? filetime_to_string(ftCreate) : L"";

    std::wcout << L"{\n";
    std::wcout << L"  \"pid\": " << pid << L",\n";

    if (hasParent) {
        std::wcout << L"  \"ppid\": " << ppid << L",\n";
    }
    else {
        std::wcout << L"  \"ppid\": null,\n";
    }

    std::wcout << L"  \"image_path\": \"" << json_escape(hasPath ? imagePath : L"") << L"\",\n";
    std::wcout << L"  \"create_time\": \"" << json_escape(createStr) << L"\",\n";

    if (hasMem) {
        std::wcout << L"  \"working_set_bytes\": " << (unsigned long long)workingSet << L",\n";
    }
    else {
        std::wcout << L"  \"working_set_bytes\": null,\n";
    }

    std::wcout << L"  \"module_count\": " << modules.size() << L",\n";

    std::wcout << L"  \"modules\": [\n";
    for (size_t i = 0; i < modules.size(); ++i) {
        std::wcout << L"    \"" << json_escape(modules[i]) << L"\"";
        if (i + 1 < modules.size()) std::wcout << L",";
        std::wcout << L"\n";
    }
    std::wcout << L"  ]\n";
    std::wcout << L"}\n";
}


void print_usage() {
    std::wcout << L"Usage:\n"
        << L"    ChildEDR.exe <pid>\n"
        << L"Example:\n"
        << L"    ChildEDR.exe 1234\n";
}

int wmain(int argc, wchar_t* argv[])
{
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    if (argc < 2) {
        std::wcerr << L"[ChildEDR] Not enough arguments.\n";
        print_usage();
        return 1;
    }

    long pidLong = _wtol(argv[1]);
    if (pidLong <= 0 || pidLong > 0x7FFFFFFF) {
        std::wcerr << L"[ChildEDR] Invalid PID: " << argv[1] << L"\n";
        return 1;
    }

    DWORD pid = (DWORD)pidLong;

    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) {
        std::wcerr << L"[ChildEDR] Cannot open process " << pid
            << L". GetLastError = " << GetLastError() << L"\n";
        return 2;
    }
    CloseHandle(h);

    print_scan_result_json(pid);

    return 0;
}
