#pragma once

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
        #if defined(__arm__)
            return true;
        #else
            return false;
        #endif
    }

    static bool isBpi() { return armBox() == lit("bpi"); }
    static bool isRaspberryPi() { return armBox() == lit("rpi"); }
    static bool isNx1() { return armBox() == lit("nx1"); }
    static bool isAndroid() { return applicationPlatform() == lit("android"); }
    static bool isIos() { return applicationPlatform() == lit("ios"); }
    static bool isMobile() { return isAndroid() || isIos(); }
    static bool isLinux() { return applicationPlatform() == lit("linux"); }
    static bool isWindows() { return applicationPlatform() == lit("windows"); }
    static bool isMacOsX() { return applicationPlatform() == lit("macosx"); }

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

} // namespace nx
} // namespace utils
