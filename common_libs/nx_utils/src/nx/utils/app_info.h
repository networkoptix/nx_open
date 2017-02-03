#pragma once

namespace nx {
namespace utils {

class NX_UTILS_API AppInfo
{
public:
    /** History name, used as a key in windows registry, appdata folder, etc. */
    static QString applicationName();

    /** Real application name, visible to the user. */
    static QString applicationDisplayName();

    static QString applicationFullVersion();
    static bool beta();
    static QString applicationVersion();
    static QString applicationRevision();
    static QString customizationName();
    static QString applicationPlatform();
    static QString applicationArch();
    static QString armBox();

    static bool isArm() { return applicationArch() == lit("arm"); }
    static bool isBpi() { return armBox() == lit("bpi"); }
    static bool isNx1() { return armBox() == lit("nx1"); }
    static bool isAndroid() { return applicationPlatform() == lit("android"); }
    static bool isIos() { return applicationPlatform() == lit("ios"); }
    static bool isMobile() { return isAndroid() || isIos(); }
};

} // namespace nx
} // namespace utils
