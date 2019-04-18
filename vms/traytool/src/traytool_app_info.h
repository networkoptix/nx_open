#pragma once

struct QnTraytoolAppInfo
{
    /** Real application name, visible to the user. */
    static QString applicationName();

    /** Client registry key. */
    static QString clientName();

    static QString mediaServerRegistryKey();
};
