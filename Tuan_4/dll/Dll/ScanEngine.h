#pragma once
#include <windows.h>

BOOL __stdcall ScanFile(
    const wchar_t* filePath,
    BOOL* isDangerous,
    wchar_t* message,
    DWORD messageLen 
);
