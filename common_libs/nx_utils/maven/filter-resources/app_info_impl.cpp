//
// This file is generated. Go to pom.xml.
//
#include "nx/utils/app_info.h"

namespace nx {
namespace utils {

bool AppInfo::beta()
{
    static const auto betaString = QStringLiteral("${beta}").toLower();
    static const bool beta =
        (betaString == lit("on") || betaString == lit("true"));
    return beta;
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

QString AppInfo::applicationPlatform()
{
    return QStringLiteral("${platform}");
}

QString AppInfo::applicationArch()
{
    return QStringLiteral("${arch}");
}

QString AppInfo::armBox()
{
    // TODO: #akolesnikov: For now, box value has sense on ARM devices only.
    // On other platforms it is used by the build system for internal purposes.
    if (isArm())
        return QStringLiteral("${box}");
    else
        return QString();
}


} // namespace nx
} // namespace utils
