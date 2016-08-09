#pragma once

struct QnApplauncherAppInfo
{
    static QString applicationName();
    static QString clientBinaryName();
    static QString mirrorListUrl();

    /** Directory where all software is installed by default. */
    static QString installationRoot();
    static QString applicationFolderName();
};
