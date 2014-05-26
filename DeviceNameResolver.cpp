#include "DeviceNameResolver.h"
#include "DeviceNameResolverInternal.h"
#include "DynBuf.h"
#include "NativeWinApi.h"

extern "C" __declspec(dllexport) bool DevicePathToPathW(const wchar_t* szDevicePath, wchar_t* szPath, size_t nSize)
{
    DeviceNameResolver deviceNameResolver;
    wchar_t targetPath[MAX_PATH]=L"";
    if(!deviceNameResolver.resolveDeviceLongNameToShort(szDevicePath, targetPath))
        return false;
    wcscpy_s(szPath, nSize/sizeof(wchar_t), targetPath);
    return true;
}

extern "C" __declspec(dllexport) bool DevicePathToPathA(const char* szDevicePath, char* szPath, size_t nSize)
{
    size_t len = strlen(szDevicePath);
    DynBuf newDevicePathBuf((len+1)*sizeof(wchar_t));
    wchar_t* newDevicePath = (wchar_t*)newDevicePathBuf.GetPtr();
    *newDevicePath=L'\0';
    if(MultiByteToWideChar(CP_ACP, NULL, szDevicePath, -1, newDevicePath, (int)len+1))
    {
        DynBuf newPathBuf(nSize*sizeof(wchar_t));
        wchar_t* newPath = (wchar_t*)newPathBuf.GetPtr();
        if(!DevicePathToPathW(newDevicePath, newPath, nSize*sizeof(wchar_t)))
            return false;
        if(!WideCharToMultiByte(CP_ACP, NULL, newPath, -1, szPath, (int)wcslen(newPath)+1, NULL, NULL))
            return false;
    }
    return true;
}

__declspec(dllexport) bool DevicePathFromFileHandleW(HANDLE hFile, wchar_t* szDevicePath, size_t nSize)
{
    NativeWinApi::initialize();
    ULONG ReturnLength;
    bool bRet=false;
    if(NativeWinApi::NtQueryObject(hFile, ObjectNameInformation, 0, 0, &ReturnLength)==STATUS_INFO_LENGTH_MISMATCH)
    {
        POBJECT_NAME_INFORMATION NameInformation=(POBJECT_NAME_INFORMATION)GlobalAlloc(0, ReturnLength);
        if(NativeWinApi::NtQueryObject(hFile, ObjectNameInformation, NameInformation, ReturnLength, 0)==STATUS_SUCCESS)
        {
            NameInformation->Name.Buffer[NameInformation->Name.Length/2]=L'\0'; //null-terminate the UNICODE_STRING
            if(wcslen(NameInformation->Name.Buffer)<nSize)
            {
                wcscpy_s(szDevicePath, nSize/sizeof(wchar_t), NameInformation->Name.Buffer);
                bRet=true;
            }
        }
        GlobalFree(NameInformation);
    }
    return bRet;
}

__declspec(dllexport) bool DevicePathFromFileHandleA(HANDLE hFile, char* szDevicePath, size_t nSize)
{
    DynBuf newDevicePathBuf(nSize*sizeof(wchar_t));
    wchar_t* newDevicePath = (wchar_t*)newDevicePathBuf.GetPtr();
    if(!DevicePathFromFileHandleW(hFile, newDevicePath, nSize*sizeof(wchar_t)))
        return false;
    if(!WideCharToMultiByte(CP_ACP, NULL, newDevicePath, -1, szDevicePath, (int)wcslen(newDevicePath)+1, NULL, NULL))
        return false;
    return true;
}

__declspec(dllexport) bool PathFromFileHandleW(HANDLE hFile, wchar_t* szPath, size_t nSize)
{
    typedef DWORD (WINAPI* GETFINALPATHNAMEBYHANDLEW) (
        IN HANDLE hFile,
        OUT wchar_t* lpszFilePath,
        IN DWORD cchFilePath,
        IN DWORD dwFlags
        );
    static GETFINALPATHNAMEBYHANDLEW GetFPNBHW=(GETFINALPATHNAMEBYHANDLEW)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetFinalPathNameByHandleW");
    if(GetFPNBHW)
    {
        if(!GetFPNBHW(hFile, szPath, (DWORD)(nSize/sizeof(wchar_t)), 0))
            return false;
        wcscpy_s(szPath, nSize, &szPath[4]); //remove "\\?\"
        return true;
    }
    if(!DevicePathFromFileHandleW(hFile, szPath, nSize))
        return false;
    return DevicePathToPathW(szPath, szPath, nSize);
}

__declspec(dllexport) bool PathFromFileHandleA(HANDLE hFile, char* szPath, size_t nSize)
{
    typedef DWORD (WINAPI* GETFINALPATHNAMEBYHANDLEA) (
        IN HANDLE hFile,
        OUT char* lpszFilePath,
        IN DWORD cchFilePath,
        IN DWORD dwFlags
        );
    static GETFINALPATHNAMEBYHANDLEA GetFPNBHA=(GETFINALPATHNAMEBYHANDLEA)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetFinalPathNameByHandleA");
    if(GetFPNBHA)
    {
        if(!GetFPNBHA(hFile, szPath, (DWORD)nSize, 0))
            return false;
        strcpy_s(szPath, nSize, &szPath[4]); //remove "\\?\"
        return true;
    }
    if(!DevicePathFromFileHandleA(hFile, szPath, nSize))
        return false;
    return DevicePathToPathA(szPath, szPath, nSize);
}
