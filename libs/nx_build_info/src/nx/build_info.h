// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/reflect/enum_instrument.h>

namespace nx::build_info {

NX_REFLECTION_ENUM_CLASS(PublicationType,
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
    release
)

/** Publication type in form of suffix - empty for release, starts with defis for other types. */
NX_BUILD_INFO_API QString publicationTypeSuffix();

NX_BUILD_INFO_API PublicationType publicationType();

/** Current repository revision id. */
NX_BUILD_INFO_API QString revision();

/** Current release version of the VMS application suite. */
NX_BUILD_INFO_API QString vmsVersion();

/** Current release version of the mobile client. */
NX_BUILD_INFO_API QString mobileClientVersion();

/** Build numbers are through for all projects. */
NX_BUILD_INFO_API int buildNumber();

/** Id of the compiler application was built with. */
NX_BUILD_INFO_API QString applicationCompiler();

NX_BUILD_INFO_API QString applicationArch();
NX_BUILD_INFO_API QString applicationPlatform();
NX_BUILD_INFO_API QString applicationPlatformNew();
NX_BUILD_INFO_API QString applicationPlatformModification();

NX_BUILD_INFO_API QString ffmpegVersion();
NX_BUILD_INFO_API QString boostVersion();

/** Addition to the version string for MetaVMS, or an empty string for non-MetaVMS builds. */
NX_BUILD_INFO_API QString usedMetaVersion();

NX_BUILD_INFO_API bool isArm();

NX_BUILD_INFO_API bool isAndroid();
NX_BUILD_INFO_API bool isIos();
NX_BUILD_INFO_API bool isMobile();
NX_BUILD_INFO_API bool isLinux();
NX_BUILD_INFO_API bool isWindows();
NX_BUILD_INFO_API bool isMacOsX();

} // namespace nx::build_info
