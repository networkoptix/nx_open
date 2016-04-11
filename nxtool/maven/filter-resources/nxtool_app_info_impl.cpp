#include <base/nxtool_app_info.h>

namespace rtu
{
    QString ApplicationInfo::applicationDisplayName()
    {
        return QStringLiteral("${product.display.title}");
    }

    QString ApplicationInfo::applicationVersion()
    {
        return QStringLiteral("${parsedVersion.majorVersion}.${parsedVersion.minorVersion}.${parsedVersion.incrementalVersion}.${buildNumber}");
    }

    QString ApplicationInfo::applicationRevision()
    {
        return QStringLiteral("${changeSet}");
    }

    bool ApplicationInfo::isBeta()
    {
        return QStringLiteral("${beta}") == QStringLiteral("true");
    }

    QString ApplicationInfo::supportUrl()
    {
        return QStringLiteral("${company.support.link}");
    }

    QString ApplicationInfo::companyUrl()
    {
        return QStringLiteral("${company.url}");
    }
}
