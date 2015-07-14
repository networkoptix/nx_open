#include "system_information.h"

#include <QtCore/QRegExp>

#include <utils/common/model_functions.h>

#include <utils/common/app_info.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnSystemInformation, (ubjson)(xml)(json)(datastream)(eq)(hash), QnSystemInformation_Fields, (optional, true))

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

#if defined(Q_OS_WIN)

    #include <windows.h>

    static QString platformModification() {
        /*
        OSVERSIONINFO osvi;
        ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

        if (GetVersionEx(&osvi))
        {
            const std::string sp(osvi.szCSDVersion);
            const auto version = lit("%1.%2").arg(osvi.dwMajorVersion)
                                             .arg(osvi.dwMinorVersion);
            if (sp.size())
                return lit("%1 %2").arg(version).arg(QString::fromStdString(sp));

            return version;
        }
        */

        return QnAppInfo::applicationPlatformModification();
    }

#elif defined(Q_OS_LINUX)

    #include <fstream>

    static QString platformModification() {
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

        return QnAppInfo::applicationPlatformModification();
    }

#elif defined(Q_OS_OSX)

    #include <sys/types.h>
    #include <sys/sysctl.h>

    static QString platformModification() {
        /*
        char osrelease[256];
        int mibMem[2] = { CTL_KERN, KERN_OSRELEASE };
        size_t size = sizeof(osrelease);
        if (sysctl(mibMem, 2, osrelease, &size, NULL, 0) != -1)
            return QString::fromStdString(osrelease);
        */

        return QnAppInfo::applicationPlatformModification();
    }

#else

    static QString platformModification() {
        return QnAppInfo::applicationPlatformModification();
    }

#endif

QnSystemInformation QnSystemInformation::currentSystemInformation() {
    return QnSystemInformation(QnAppInfo::applicationPlatform(),
                               QnAppInfo::applicationArch(),
                               platformModification());
}
