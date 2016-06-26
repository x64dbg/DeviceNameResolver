#include "DeviceNameResolver.h"
#include "DeviceNameResolverInternal.h"
#include "DynBuf.h"
#include "NativeWinApi.h"

extern "C" __declspec(dllexport) bool DevicePathToPathW(const wchar_t* szDevicePath, wchar_t* szPath, size_t nSizeInChars)
{
    DeviceNameResolver deviceNameResolver;
    wchar_t targetPath[MAX_PATH] = L"";
    if(!deviceNameResolver.resolveDeviceLongNameToShort(szDevicePath, targetPath))
        return false;
    wcsncpy_s(szPath, nSizeInChars, targetPath, _TRUNCATE);
    return true;
}

extern "C" __declspec(dllexport) bool DevicePathToPathA(const char* szDevicePath, char* szPath, size_t nSizeInChars)
{
    size_t len = strlen(szDevicePath);
    DynBuf newDevicePathBuf((len + 1) * sizeof(wchar_t));
    wchar_t* newDevicePath = (wchar_t*)newDevicePathBuf.GetPtr();
    *newDevicePath = L'\0';
    if(MultiByteToWideChar(CP_ACP, NULL, szDevicePath, -1, newDevicePath, int(len + 1)))
    {
        DynBuf newPathBuf((nSizeInChars + 1) * sizeof(wchar_t));
        wchar_t* newPath = (wchar_t*)newPathBuf.GetPtr();
        if(!DevicePathToPathW(newDevicePath, newPath, nSizeInChars))
            return false;
        if(!WideCharToMultiByte(CP_ACP, NULL, newPath, -1, szPath, int(wcslen(newPath)) + 1, NULL, NULL))
            return false;
    }
    return true;
}

__declspec(dllexport) bool DevicePathFromFileHandleW(HANDLE hFile, wchar_t* szDevicePath, size_t nSizeInChars)
{
    NativeWinApi::initialize();
    ULONG ReturnLength;
    bool bRet = false;
    if(NativeWinApi::NtQueryObject(hFile, ObjectNameInformation, 0, 0, &ReturnLength) == STATUS_INFO_LENGTH_MISMATCH)
    {
        ReturnLength += 0x2000; //on Windows XP SP3 ReturnLength will not be set just add some buffer space to fix this
        POBJECT_NAME_INFORMATION NameInformation = POBJECT_NAME_INFORMATION(GlobalAlloc(0, ReturnLength));
        if(NativeWinApi::NtQueryObject(hFile, ObjectNameInformation, NameInformation, ReturnLength, 0) == STATUS_SUCCESS)
        {
            NameInformation->Name.Buffer[NameInformation->Name.Length / 2] = L'\0'; //null-terminate the UNICODE_STRING
            wcsncpy_s(szDevicePath, nSizeInChars, NameInformation->Name.Buffer, _TRUNCATE);
            bRet = true;
        }
        GlobalFree(NameInformation);
    }
    if(!bRet)
        return false;
    if(_wcsnicmp(szDevicePath, L"\\Device\\LanmanRedirector\\", 25) == 0) // Win XP
    {
        wcsncpy_s(szDevicePath, nSizeInChars, L"\\\\", _TRUNCATE);
        wcsncat_s(szDevicePath, nSizeInChars, &szDevicePath[25], _TRUNCATE);
    }
    else if(_wcsnicmp(szDevicePath, L"\\Device\\Mup\\", 12) == 0) // Win 7
    {
        wcsncpy_s(szDevicePath, nSizeInChars, L"\\\\", _TRUNCATE);
        wcsncat_s(szDevicePath, nSizeInChars, &szDevicePath[12], _TRUNCATE);
    }
    return true;
}

__declspec(dllexport) bool DevicePathFromFileHandleA(HANDLE hFile, char* szDevicePath, size_t nSizeInChars)
{
    DynBuf newDevicePathBuf((nSizeInChars + 1) * sizeof(wchar_t));
    wchar_t* newDevicePath = (wchar_t*)newDevicePathBuf.GetPtr();
    if(!DevicePathFromFileHandleW(hFile, newDevicePath, nSizeInChars))
        return false;
    if(!WideCharToMultiByte(CP_ACP, NULL, newDevicePath, -1, szDevicePath, int(wcslen(newDevicePath)) + 1, NULL, NULL))
        return false;
    return true;
}

__declspec(dllexport) bool PathFromFileHandleW(HANDLE hFile, wchar_t* szPath, size_t nSizeInChars)
{
    typedef DWORD (WINAPI * GETFINALPATHNAMEBYHANDLEW)(
        IN HANDLE /*hFile*/,
        OUT wchar_t* /*lpszFilePath*/,
        IN DWORD /*cchFilePath*/,
        IN DWORD /*dwFlags*/
    );
    static GETFINALPATHNAMEBYHANDLEW GetFPNBHW = GETFINALPATHNAMEBYHANDLEW(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetFinalPathNameByHandleW"));
    if(GetFPNBHW && GetFPNBHW(hFile, szPath, DWORD(nSizeInChars), 0))
    {
        if(_wcsnicmp(szPath, L"\\\\?\\UNC\\", 8) == 0) // Server path
        {
            wcsncpy_s(szPath, nSizeInChars, L"\\\\", _TRUNCATE);
            wcsncat_s(szPath, nSizeInChars, &szPath[8], _TRUNCATE);
        }
        else if(_wcsnicmp(szPath, L"\\\\?\\", 4) == 0 && szPath[5] == L':') // Drive path
        {
            wcsncpy_s(szPath, nSizeInChars, &szPath[4], _TRUNCATE);
        }
        return true;
    }
    if(!DevicePathFromFileHandleW(hFile, szPath, nSizeInChars))
        return false;
    std::wstring oldPath(szPath);
    if(!DevicePathToPathW(szPath, szPath, nSizeInChars))
        wcsncpy_s(szPath, nSizeInChars, oldPath.c_str(), _TRUNCATE);
    return true;
}

__declspec(dllexport) bool PathFromFileHandleA(HANDLE hFile, char* szPath, size_t nSizeInChars)
{
    DynBuf newDevicePathBuf((nSizeInChars + 1) * sizeof(wchar_t));
    wchar_t* newDevicePath = (wchar_t*)newDevicePathBuf.GetPtr();
    if(!PathFromFileHandleW(hFile, newDevicePath, nSizeInChars))
        return false;
    if(!WideCharToMultiByte(CP_ACP, NULL, newDevicePath, -1, szPath, int(wcslen(newDevicePath)) + 1, NULL, NULL))
        return false;
    return true;
}
