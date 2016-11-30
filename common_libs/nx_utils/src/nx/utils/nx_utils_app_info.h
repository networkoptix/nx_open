#pragma once

struct NX_UTILS_API NxUtilsAppInfo
{
    /** History name, used as a key in windows registry, appdata folder, etc. */
    static QString applicationName();

    /** Real application name, visible to the user. */
    static QString applicationDisplayName();

    static QString applicationFullVersion();

    static bool beta();

    static QString applicationVersion();

    static QString applicationRevision();

    static QString customizationName();
};
