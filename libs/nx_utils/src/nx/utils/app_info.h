#pragma once

#include <QtCore/QString>

#include <nx/utils/literal.h>

namespace nx {
namespace utils {

class NX_UTILS_API AppInfo
{
public:
    static QString applicationFullVersion();
    static bool beta();
    static QString applicationVersion();
    static QString applicationRevision();
    static QString customizationName();
    static QString applicationPlatform();
    static QString applicationArch();
    static QString armBox();

    static QString productName();
    static QString productNameShort();
    static QString productNameLong();
    static QString organizationName();
    static QString linuxOrganizationName();
    static QString organizationNameForSettings();

    static bool isArm()
    {
        #if defined(__arm__) || defined(__aarch64__)
            return true;
        #else
            return false;
        #endif
    }

    static bool isBpi() { return armBox() == QStringLiteral("bpi"); }
    static bool isRaspberryPi() { return armBox() == QStringLiteral("rpi"); }
    static bool isNx1() { return armBox() == QStringLiteral("nx1"); }
    static bool isAndroid() { return applicationPlatform() == QStringLiteral("android"); }
    static bool isIos() { return applicationPlatform() == QStringLiteral("ios"); }
    static bool isMobile() { return isAndroid() || isIos(); }
    static bool isLinux() { return applicationPlatform() == QStringLiteral("linux"); }
    static bool isWindows() { return applicationPlatform() == QStringLiteral("windows"); }
    static bool isMacOsX() { return applicationPlatform() == QStringLiteral("macosx"); }

    static bool isWin64()
    {
        if (!isWindows())
            return false;

        #if defined(Q_OS_WIN64)
            return true;
        #else
            return false;
        #endif
    }

    static bool isWin32()
    {
        return isWindows() && !isWin64();
    }
};

} // namespace utils
} // namespace nx
