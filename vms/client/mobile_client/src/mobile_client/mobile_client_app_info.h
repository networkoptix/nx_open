#pragma once

struct QnMobileClientAppInfo
{
    /** History name, used as a key in windows registry, appdata folder, etc. */
    static QString applicationName();

    /** Real application name, visible to the user. */
    static QString applicationDisplayName();

    static QString applicationVersion();

    /** Is lite mode enabled by default. */
    static bool defaultLiteMode();

    static QString oldAndroidClientLink();
    static QString oldIosClientLink();

    static QString oldAndroidAppId();

    static QString liteDeviceName();
};
