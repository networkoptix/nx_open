/**@file
 * Being located in the build directory, this file is generated by CMake from the source file
 * with the same name, substituting CMake variables.
 */

#include "nx/utils/app_info.h"

namespace nx::utils {

bool AppInfo::beta()
{
    return ${beta};
}

QString AppInfo::applicationVersion()
{
    return
        "${parsedVersion.majorVersion}"
        ".${parsedVersion.minorVersion}"
        ".${parsedVersion.incrementalVersion}"
        ".${buildNumber}";
}

QString AppInfo::applicationRevision()
{
    return "${changeSet}";
}

QString AppInfo::customizationName()
{
    return "${customization}";
}

QString AppInfo::applicationFullVersion()
{
    static const QString kBeta = beta() ? "-beta" : QString();
    static const QString kFullVersion = QString("%1-%2-%3%4")
        .arg(applicationVersion())
        .arg(applicationRevision())
        .arg(customizationName().replace(L' ', L'_'))
        .arg(kBeta);

    return kFullVersion;
}

QString AppInfo::applicationPlatform()
{
    return "${platform}";
}

QString AppInfo::applicationPlatformNew()
{
    return "${platform_new}";
}

QString AppInfo::applicationArch()
{
    return "${arch}";
}

QString AppInfo::organizationName()
{
    return "${company.name}";
}

QString AppInfo::linuxOrganizationName()
{
    return "${deb.customization.company.name}";
}

QString AppInfo::organizationNameForSettings()
{
    #if defined(_WIN32)
        return organizationName();
    #else
        return linuxOrganizationName();
    #endif
}

QString AppInfo::productNameShort()
{
    return "${product.name.short}";
}

QString AppInfo::productNameLong()
{
    return "${display.product.name}";
}

QString AppInfo::productName()
{
    #if defined(_WIN32)
        return "${product.name}";
    #else
        return "${product.appName}";
    #endif
}

QString AppInfo::armBox()
{
    // TODO: #akolesnikov For now, the value of `box` has sense on ARM devices only.
    //     On other platforms it is used by the build system for internal purposes.
    if (isArm())
        return "${box}";
    else
        return QString();
}

bool AppInfo::isEdgeServer()
{
    return ${isEdgeServer};
}

bool AppInfo::isArm()
{
    #if defined(__arm__) || defined(__aarch64__)
        return true;
    #else
        return false;
    #endif
}

bool AppInfo::isWin64()
{
    if (!isWindows())
        return false;

    #if defined(Q_OS_WIN64)
        return true;
    #else
        return false;
    #endif
}

bool AppInfo::isWin32()
{
    return isWindows() && !isWin64();
}

bool AppInfo::isNx1()
{
    return armBox() == "bpi";
}

bool AppInfo::isAndroid()
{
    return applicationPlatform() == "android";
}

bool AppInfo::isIos()
{
    return applicationPlatform() == "ios";
}

bool AppInfo::isMobile()
{
    return isAndroid() || isIos();
}

bool AppInfo::isLinux()
{
    return applicationPlatform() == "linux";
}

bool AppInfo::isWindows()
{
    return applicationPlatform() == "windows";
}

bool AppInfo::isMacOsX()
{
    return applicationPlatform() == "macosx";
}

} // namespace nx::utils
