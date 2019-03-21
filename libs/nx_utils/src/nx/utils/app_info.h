#pragma once

#include <QtCore/QString>

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

    static bool isArm();
    static bool isWin64();
    static bool isWin32();

    static bool isBpi();
    static bool isRaspberryPi();
    static bool isNx1();
    static bool isAndroid();
    static bool isIos();
    static bool isMobile();
    static bool isLinux();
    static bool isWindows();
    static bool isMacOsX();
};

} // namespace utils
} // namespace nx
