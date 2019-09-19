#pragma once

#include <QtCore/QString>

namespace nx::utils {

class NX_UTILS_API AppInfo
{
public:

    /**
     * Customization VMS name. Used for the display purposes.
     */
    static QString vmsName();

    /**
     * Customization brand. Used for the license compatibility checks.
     */
    static QString brand();

    static QString applicationFullVersion();
    static bool beta();
    static QString applicationVersion();
    static QString applicationRevision();
    static QString customizationName();
    static QString applicationPlatform();
    static QString applicationPlatformNew();
    static QString applicationArch();
    static QString armBox();

    static QString organizationName();
    static QString linuxOrganizationName();
    static QString organizationNameForSettings();

    static QString supportAddress();
    static QString licensingAddress();

    static bool isEdgeServer();
    static bool isArm();
    static bool isWin64();
    static bool isWin32();

    static bool isNx1();
    static bool isAndroid();
    static bool isIos();
    static bool isMobile();
    static bool isLinux();
    static bool isWindows();
    static bool isMacOsX();
};

} // namespace nx::utils
