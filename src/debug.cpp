#include "pch.h"
#include "debug.h"
#include <pathcch.h>
#include <ctime>


static void WriteToLogFile(const char* pszMessage) {
    wchar_t sPath[MAX_PATH];
    GetModuleFileNameW(nullptr, sPath, MAX_PATH);
    PathCchRemoveFileSpec(sPath, MAX_PATH);

    time_t now = time(nullptr);
    struct tm localtime;
    localtime_s(&localtime, &now);

    wchar_t sFileName[MAX_PATH];
    wcsftime(sFileName, MAX_PATH, L"debug_%Y-%m-%d.txt", &localtime);
    PathCchAppend(sPath, MAX_PATH, sFileName);

    FILE* file = nullptr;
    if (_wfopen_s(&file, sPath, L"a") != 0 || !file) {
        return;
    }

    char sTimeInfo[64];
    strftime(sTimeInfo, sizeof(sTimeInfo), "%H:%M:%S", &localtime);
    fprintf_s(file, "[%s] %s\n", sTimeInfo, pszMessage);
    fclose(file);
}


void DebugMessage(const char* pszFormat, ...) {
    char pszDest[1024];
    size_t cbDest = 1024 * sizeof(char);
    va_list argList;
    va_start(argList, pszFormat);
    StringCbVPrintfA(pszDest, cbDest, pszFormat, argList);
    OutputDebugStringA(pszDest);
    WriteToLogFile(pszDest);
    va_end(argList);
}

void ErrorMessage(const char* pszFormat, ...) {
    char pszDest[1024];
    size_t cbDest = 1024 * sizeof(char);
    va_list argList;
    va_start(argList, pszFormat);
    StringCbVPrintfA(pszDest, cbDest, pszFormat, argList);
    MessageBox(nullptr, pszDest, "Error", MB_ICONERROR);
    va_end(argList);
}