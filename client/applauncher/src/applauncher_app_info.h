#pragma once

#include <QtCore/QString>

struct QnApplauncherAppInfo
{
    static QString applicationName();
    static QString clientBinaryName();

    /** Directory where all software is installed by default. */
    static QString installationRoot();

    static QString productName();

    static QString bundleName();
};
