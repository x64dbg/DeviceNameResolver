#include "DeviceNameResolver.h"
#include "DeviceNameResolverInternal.h"
#include "DynBuf.h"

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
        if(!WideCharToMultiByte(CP_ACP, NULL, newPath, -1, szPath, (int)wcslen(newPath) + 1, NULL, NULL))
            return false;
    }
    return true;
}
