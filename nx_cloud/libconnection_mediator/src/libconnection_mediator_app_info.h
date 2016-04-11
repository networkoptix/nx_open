#pragma once

struct QnLibConnectionMediatorAppInfo
{
    /** History name, used as a key in windows registry, appdata folder, etc. */
    static QString applicationName();

    /** Real application name, visible to the user. */
    static QString applicationDisplayName();
};
