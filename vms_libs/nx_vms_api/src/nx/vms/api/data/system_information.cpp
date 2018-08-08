#include "system_information.h"

#include <QtCore/QRegExp>

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemInformation),
    (eq)(hash)(ubjson)(json)(xml)(csv_record)(datastream),
    _Fields)

SystemInformation::SystemInformation(const QString& platform, const QString& arch,
    const QString& modification)
    :
    arch(arch),
    platform(platform),
    modification(modification),
    version(runtimeOsVersion())
{
}

SystemInformation::SystemInformation(const QString& infoString)
{
    QRegExp infoRegExp(QLatin1String("(\\S+)\\s+(\\S+)\\s*(\\S*)"));
    if (infoRegExp.exactMatch(infoString))
    {
        platform = infoRegExp.cap(1);
        arch = infoRegExp.cap(2);
        modification = infoRegExp.cap(3);
        version = runtimeOsVersion();
    }
}

QString SystemInformation::toString() const
{
    const auto result = QStringLiteral("%1 %2").arg(platform, arch);
    return modification.isEmpty()
        ? result
        : QStringLiteral("%1 %2").arg(result, modification);
}

bool SystemInformation::isValid() const
{
    return !platform.isEmpty() && !arch.isEmpty();
}

} // namespace api
} // namespace vms
} // namespace nx

#if defined(Q_OS_WIN)

#include <windows.h>

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

QString nx::vms::api::SystemInformation::currentSystemRuntime()
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

QString nx::vms::api::SystemInformation::runtimeOsVersion()
{
    OSVERSIONINFOEXW osvi;
    if (!winVersion(&osvi))
        return QLatin1String("0");

    return QString::number(osvi.dwMajorVersion) + "." +
        QString::number(osvi.dwMinorVersion) + "." +
        QString::number(osvi.dwBuildNumber);
}

#elif defined(Q_OS_LINUX)

#include <fstream>

static QMap<QString, QString> osReleaseContents()
{
    QFile f("/etc/os-release");
    if (!f.open(QIODevice::ReadOnly))
        return QMap<QString, QString>();

    QTextStream stream(f.readAll());
    QMap<QString, QString> result;
    while (!stream.atEnd())
    {
        QString line = stream.readLine();
        if (line.isEmpty())
            continue;

        QRegExp keyValueRegExp("^(.+)=(.+)$");
        int reIndex = keyValueRegExp.indexIn(line);
        if (reIndex == -1 || keyValueRegExp.captureCount() != 2)
            continue;

        auto captureList = keyValueRegExp.capturedTexts();
        result.insert(captureList[1], captureList[2].replace("\"", ""));
    }

    return result;
}

QString nx::vms::api::SystemInformation::currentSystemRuntime()
{
    const auto contents = osReleaseContents();
    const QString kPrettyNameKey = "PRETTY_NAME";

    return contents.contains(kPrettyNameKey)
        ? contents[kPrettyNameKey] : "GNU-Linux without /etc/os-release";
}

QString nx::vms::api::SystemInformation::runtimeOsVersion()
{
    const auto contents = osReleaseContents();
    if (contents.isEmpty())
        return "0";

    if (contents.contains("ID"))
    {
        const auto idValue = contents["ID"].toLower();
        if (idValue == "ubuntu")
        {
            QRegExp versionRegExp("[^0-9]*([0-9]+\\.[0-9+]+\\.[0-9]+)[^0-9]*");
            int reIndex = versionRegExp.indexIn(idValue);
            if (reIndex == -1 || versionRegExp.captureCount() != 1)
                return "0";

            return versionRegExp.capturedTexts()[1];
        }

        if (contents.contains("ID_LIKE"))
        {
            const auto idLikeValue = contents["ID_LIKE"].toLower();
            if (idLikeValue != "ubuntu")
                return "0";

            if (!contents.contains("UBUNTU_CODENAME"))
                return "0";

            const auto ubuntuCodename = contents["UBUNTU_CODENAME"].toLower();
            if (ubuntuCodename.contains("trusty"))
                return "14.04";
            if (ubuntuCodename.contains("xenial"))
                return "16.04";
            if (ubuntuCodename.contains("bionic"))
                return "18.04";
            if (ubuntuCodename.contains("tahr"))
                return "14.04";
            if (ubuntuCodename.contains("precise"))
                return "12.04";
            if (ubuntuCodename.contains("quantal"))
                return "12.10";
            if (ubuntuCodename.contains("raring"))
                return "13.04";
            if (ubuntuCodename.contains("saucy"))
                return "13.10";
            if (ubuntuCodename.contains("utopic"))
                return "14.10";
            if (ubuntuCodename.contains("vivid"))
                return "15.04";
            if (ubuntuCodename.contains("willy"))
                return "15.10";
            if (ubuntuCodename.contains("yakkety"))
                return "16.10";
            if (ubuntuCodename.contains("zesty"))
                return "17.04";
            if (ubuntuCodename.contains("artful"))
                return "17.10";
        }
    }

    return "0";
}

#elif defined(Q_OS_OSX)

#include <sys/types.h>
#include <sys/sysctl.h>

QString nx::vms::api::SystemInformation::currentSystemRuntime()
{
    char osrelease[256];
    int mibMem[2] = {CTL_KERN, KERN_OSRELEASE};
    size_t size = sizeof(osrelease);
    if (sysctl(mibMem, 2, osrelease, &size, NULL, 0) != -1)
        return QString::fromStdString(osrelease);

    return QLatin1String("OSX without KERN_OSRELEASE");
}

#else

QString nx::vms::api::SystemInformation::currentSystemRuntime()
{
    return QLatin1String("Unknown");
}

#endif
