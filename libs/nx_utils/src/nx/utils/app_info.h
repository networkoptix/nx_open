#pragma once

#include <QtCore/QString>

namespace nx::utils {

enum class PublicationType
{
    /** Local developer build. */
    local,

    /** Private build for QA team. */
    private_build,

    /** Private patch for a single setup. */
    private_patch,

    /** Regular monthy patch. */
    patch,

    /** Public beta version. */
    beta,

    /** Public release candidate. */
    rc,

    /** Public release. */
    release,
};

class NX_UTILS_API AppInfo
{
public:
    static QString toString(PublicationType publicationType);

    /** Publication type in form of suffix - empty for release, starts with defis for other types. */
    static QString publicationTypeSuffix();

    static PublicationType publicationType();

    /**
     * Customization VMS name. Used for the display purposes.
     */
    static QString vmsName();

    /**
     * Customization brand. Used for the license compatibility checks.
     */
    static QString brand();

    static QString applicationFullVersion();
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

    static QString homePage();
    static QString supportPage();

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
