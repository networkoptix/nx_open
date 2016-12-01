//
// This file is generated. Go to pom.xml.
//
#include "nx/utils/app_info.h"

namespace nx {
namespace utils {

QString AppInfo::applicationName()
{
    return QStringLiteral("${product.title}");
}

QString AppInfo::applicationDisplayName()
{
    return QStringLiteral("${product.display.title}");
}

bool AppInfo::beta()
{
    return ${beta};
}

QString AppInfo::applicationVersion()
{
    return QStringLiteral("${parsedVersion.majorVersion}.${parsedVersion.minorVersion}.${parsedVersion.incrementalVersion}.${buildNumber}");
}

QString AppInfo::applicationRevision()
{
    return QStringLiteral("${changeSet}");
}

QString AppInfo::customizationName()
{
    return QStringLiteral("${customization}");
}

QString AppInfo::applicationFullVersion()
{
    static const QString kBeta = beta() ? lit("-beta") : QString();
    static const QString kFullVersion = lit("%1-%2-%3%4")
        .arg(applicationVersion())
        .arg(applicationRevision())
        .arg(customizationName().replace(L' ', L'_'))
        .arg(kBeta);

    return kFullVersion;
}

} // namespace nx
} // namespace utils
