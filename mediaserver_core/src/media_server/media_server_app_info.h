#pragma once

struct QnServerAppInfo
{
    /** History name, used as a key in windows registry, appdata folder, etc. */
    static QString applicationName();

    /** Real application name, visible to the user. */
    static QString applicationDisplayName();

    /** Name of the mediaserver service. */
    static QString serviceName();
};
