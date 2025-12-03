
#include "pch.h"          
#include <cwchar>

static void SafeWriteMessage(wchar_t* buffer, DWORD bufferLen, const wchar_t* text)
{
    if (!buffer || bufferLen == 0) return;
    wcsncpy_s(buffer, bufferLen, text, _TRUNCATE);
}


extern "C" __declspec(dllexport)
BOOL __stdcall ScanFile(
    const wchar_t* filePath,
    BOOL* isDangerous,
    wchar_t* message,
    DWORD messageLen
)
{
    if (!filePath || !isDangerous) {
        SafeWriteMessage(message, messageLen, L"[ENGINE] Invalid parameters");
        return FALSE; 
    }
    wchar_t drive = filePath[0];
    bool onC =
        (drive == L'C' || drive == L'c') &&
        filePath[1] == L':';

    if (onC) {
        *isDangerous = FALSE;
        SafeWriteMessage(message, messageLen, L"[ENGINE] Safe: file is on drive C");
    }
    else {
        *isDangerous = TRUE;
        SafeWriteMessage(message, messageLen, L"[ENGINE] DANGEROUS: file is NOT on drive C");
    }

    return TRUE; 
}
