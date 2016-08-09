#pragma once

struct QnApplauncherAppInfo
{
    static QString applicationName();
    static QString clientBinaryName();
    static QString mirrorListUrl();

    /** Directory where all software is installed by default. */
    static QString installationRoot();

#if defined(Q_MAC_OSX)
    static QString applicationFolderName();
#endif

};
