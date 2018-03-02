#pragma once

#include <QtCore/QString>

struct QnApplauncherAppInfo
{
    static QString applicationName();
    static QString clientBinaryName();
    static QString mirrorListUrl();

    /** Directory where all software is installed by default. */
    static QString installationRoot();

#if defined(Q_OS_MACX)
    static QString productName();

    static QString bundleName();
#endif

};
