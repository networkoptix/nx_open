//
// This file is generated. Go to pom.xml.
//
#include "nx/utils/nx_utils_app_info.h"

QString NxUtilsAppInfo::applicationName()
{
    return QStringLiteral("${product.title}");
}

QString NxUtilsAppInfo::applicationDisplayName()
{
    return QStringLiteral("${product.display.title}");
}

bool NxUtilsAppInfo::beta()
{
    return ${beta};
}

QString NxUtilsAppInfo::applicationVersion()
{
    return QStringLiteral("${parsedVersion.majorVersion}.${parsedVersion.minorVersion}.${parsedVersion.incrementalVersion}.${buildNumber}");
}

QString NxUtilsAppInfo::applicationRevision()
{
    return QStringLiteral("${changeSet}");
}

QString NxUtilsAppInfo::customizationName()
{
    return QStringLiteral("${customization}");
}

QString NxUtilsAppInfo::applicationFullVersion()
{
    static const QString kBeta = beta() ? lit("-beta") : QString();
    static const QString kFullVersion = lit("%1-%2-%3%4")
        .arg(applicationVersion())
        .arg(applicationRevision())
        .arg(customizationName().replace(L' ', L'_'))
        .arg(kBeta);

    return kFullVersion;
}
