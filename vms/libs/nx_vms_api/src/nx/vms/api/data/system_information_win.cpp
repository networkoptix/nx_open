#include <windows.h>

#include "system_information.h"
#include <nx/utils/literal.h>

namespace nx::vms::api {

namespace {

static QString resolveGetVersionEx(DWORD major, DWORD minor, bool ws)
{
    if (major == 5 && minor == 0)
        return lit("2000");

    if (major == 5 && minor == 1)
        return lit("XP");

    if (major == 5 && minor == 2)
        return GetSystemMetrics(SM_SERVERR2) ? lit("Server 2003") : lit("Server 2003 R2");

    if (major == 6 && minor == 0)
        return ws ? lit("Vista") : lit("Server 2008");

    if (major == 6 && minor == 1)
        return ws ? lit("7") : lit("Server 2008 R2");

    if (major == 6 && minor == 2)
        return ws ? lit("8") : lit("Server 2012");

    if (major == 6 && minor == 3)
        return ws ? lit("8.1") : lit("Server 2012 R2");

    if (major == 10 && minor == 0)
        return ws ? lit("10") : lit("Server 2016");

    return lit("Unknown %1.%2").arg(major).arg(minor);
}

static bool winVersion(OSVERSIONINFOEXW* osvi)
{
    ZeroMemory(osvi, sizeof(osvi));
    osvi->dwOSVersionInfoSize = sizeof(*osvi);

    NTSTATUS(WINAPI *getVersion)(LPOSVERSIONINFOEXW);
    if ((FARPROC&)getVersion = GetProcAddress(GetModuleHandleA("ntdll"), "RtlGetVersion"))
    {
        if (SUCCEEDED(getVersion(osvi)))
            return true;
    }

    return false;
}

} // namespace

QString SystemInformation::currentSystemRuntime()
{
    OSVERSIONINFOEXW osvi;
    if (!winVersion(&osvi))
        return QLatin1String("Windows without RtlGetVersion");

    QString name = lit("Windows %1").arg(resolveGetVersionEx(
        osvi.dwMajorVersion, osvi.dwMinorVersion,
        osvi.wProductType == VER_NT_WORKSTATION));

    if (osvi.wServicePackMajor)
        name += lit(" sp%1").arg(osvi.wServicePackMajor);

    return name;
}

QString SystemInformation::runtimeOsVersion()
{
    OSVERSIONINFOEXW osvi;
    if (!winVersion(&osvi))
        return QLatin1String("0");

    return QString::number(osvi.dwMajorVersion) + "." +
        QString::number(osvi.dwMinorVersion) + "." +
        QString::number(osvi.dwBuildNumber);
}

} // namespace nx::vms::api