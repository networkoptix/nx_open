//
// This file is generated. Go to pom.xml.
//
#include "nx/utils/app_info.h"

namespace nx {
namespace utils {

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

QString AppInfo::applicationPlatform()
{
    return QStringLiteral("${platform}");
}

QString AppInfo::applicationArch()
{
    return QStringLiteral("${arch}");
}

QString AppInfo::organizationName()
{
    return QStringLiteral("${company.name}");
}

QString AppInfo::linuxOrganizationName()
{
    return QStringLiteral("${deb.customization.company.name}");
}

QString AppInfo::organizationNameForSettings()
{
#ifdef _WIN32
    return organizationName();
#else
    return linuxOrganizationName();
#endif
}

QString AppInfo::productNameShort()
{
    return QStringLiteral("${product.name.short}");
}

QString AppInfo::productNameLong()
{
    return QStringLiteral("${display.product.name}");
}

QString AppInfo::productName()
{
#ifdef _WIN32
    return QStringLiteral("${product.name}");
#else
    return QStringLiteral("${product.appName}");
#endif
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
