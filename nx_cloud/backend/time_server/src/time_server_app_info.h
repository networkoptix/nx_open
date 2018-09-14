#pragma once

#include <QtCore/QString>

struct TimeServerAppInfo
{
    /** Application name, used as a key in windows registry, appdata folder, etc. */
    static QString applicationName();
};
