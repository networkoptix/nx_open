#include "system_information.h"

#include <QtCore/QRegExp>

#include <nx/fusion/model_functions.h>

#include <utils/common/app_info.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnSystemInformation), (ubjson)(xml)(json)(datastream)(eq)(hash), _Fields)

QnSystemInformation::QnSystemInformation(const QString &platform, const QString &arch, const QString &modification) :
    arch(arch),
    platform(platform),
    modification(modification)
{}

QnSystemInformation::QnSystemInformation(const QString &infoString) {
    QRegExp infoRegExp(QLatin1String("(\\S+)\\s+(\\S+)\\s*(\\S*)"));
    if (infoRegExp.exactMatch(infoString)) {
        platform = infoRegExp.cap(1);
        arch = infoRegExp.cap(2);
        modification = infoRegExp.cap(3);
    }
}

QString QnSystemInformation::toString() const {
    const auto result = lit("%1 %2").arg(platform).arg(arch);
    if (!modification.isEmpty())
        return lit("%1 %2").arg(result).arg(modification);
    return result;
}

bool QnSystemInformation::isValid() const {
    return !platform.isEmpty() && !arch.isEmpty();
}

QnSystemInformation QnSystemInformation::currentSystemInformation() {
    return QnSystemInformation(QnAppInfo::applicationPlatform(),
                               QnAppInfo::applicationArch(),
                               QnAppInfo::applicationPlatformModification());
}

#if defined(Q_OS_WIN)

    #include <windows.h>

    static QString resolveGetVersionEx(DWORD major, DWORD minor, bool ws)
    {
        if (major == 5 && minor == 0) return lit("2000");
        if (major == 5 && minor == 1) return lit("XP");
        if (major == 5 && minor == 2) 
            return GetSystemMetrics(SM_SERVERR2) ? lit("Server 2003") : lit("Server 2003 R2");

        if (major == 6 && minor == 0) return ws ? lit("Vista") : lit("Server 2008");
        if (major == 6 && minor == 1) return ws ? lit("7") : lit("Server 2008 R2");
        if (major == 6 && minor == 2) return ws ? lit("8") : lit("Server 2012");
        if (major == 6 && minor == 3) return ws ? lit("8.1") : lit("Server 2012 R2");
        if (major == 10 && minor == 0) return ws ? lit("10") : lit("Server 2016");
        
        return lit("Unknown %1.%2").arg(major).arg(minor);
    }

    QString QnSystemInformation::currentSystemRuntime() {
        OSVERSIONINFOEXW osvi; 
        ZeroMemory(&osvi, sizeof(osvi));
        osvi.dwOSVersionInfoSize = sizeof(osvi);

        NTSTATUS(WINAPI *getVersion)(LPOSVERSIONINFOEXW);
        if ((FARPROC&)getVersion = GetProcAddress(GetModuleHandleA("ntdll"), "RtlGetVersion"))
        {
            if (SUCCEEDED(getVersion(&osvi)))
            {
                QString name = lit("Windows %1").arg(resolveGetVersionEx(
                    osvi.dwMajorVersion, osvi.dwMinorVersion,
                    osvi.wProductType == VER_NT_WORKSTATION));

                if (osvi.wServicePackMajor)
                    name += lit(" sp%1").arg(osvi.wServicePackMajor);

                return name;
            }
        }

        return QLatin1String("Windows without RtlGetVersion");
    }

#elif defined(Q_OS_LINUX)

    #include <fstream>

    QString QnSystemInformation::currentSystemRuntime() {
        std::ifstream osrelease("/etc/os-release");
        if (osrelease.is_open())
        {
            std::string line;
            while(std::getline(osrelease, line))
            {
                if (line.find("PRETTY_NAME") == 0)
                {
                    const auto name = line.substr(line.find("=") + 1);
                    if (name.size() > 2 && name[0] == '"')
                        // rm quotes
                        return QString::fromStdString(name.substr(1, name.size() - 2));

                    return QString::fromStdString(name);
                }
            }
        }

        return QLatin1String("GNU-Linux without /etc/os-release");
    }

#elif defined(Q_OS_OSX)

    #include <sys/types.h>
    #include <sys/sysctl.h>

    QString QnSystemInformation::currentSystemRuntime() {
        char osrelease[256];
        int mibMem[2] = { CTL_KERN, KERN_OSRELEASE };
        size_t size = sizeof(osrelease);
        if (sysctl(mibMem, 2, osrelease, &size, NULL, 0) != -1)
            return QString::fromStdString(osrelease);

        return QLatin1String("OSX without KERN_OSRELEASE");
    }

#else

    QString QnSystemInformation::currentSystemRuntime() {
        return QLatin1String("Unknown");
    }

#endif
