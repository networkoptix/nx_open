#pragma once

#include <QtCore/QString>

namespace rtu
{
    struct ApplicationInfo
    {
        static QString applicationDisplayName();
        static QString applicationVersion();
        static QString applicationRevision();
        static bool isBeta();
        static QString supportUrl();
        static QString companyUrl();
    };
}
