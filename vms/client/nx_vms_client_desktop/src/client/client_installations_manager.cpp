// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_installations_manager.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <nx/utils/log/log.h>
#include <nx/utils/software_version.h>

#include <nx/branding.h>
#include <nx/build_info.h>

QString QnClientInstallationsManager::binDirSuffix()
{
    if (nx::build_info::isLinux())
        return "bin";

    if (nx::build_info::isMacOsX())
        return "Contents/MacOS";

    return QString();
}

QString QnClientInstallationsManager::libDirSuffix()
{
    if (nx::build_info::isLinux())
        return "lib";

    if (nx::build_info::isMacOsX())
        return "Contents/Frameworks";

    return QString();
}

QStringList QnClientInstallationsManager::clientInstallRoots()
{
    QStringList result;
    QList<QFileInfo> entries;

    if (nx::build_info::isMacOsX())
    {
        const QDir installationRootDir = QDir(nx::branding::installationRoot());
        const QFileInfo bundleEntry(installationRootDir.absoluteFilePath(
            nx::branding::vmsName() + ".app"));
        if (bundleEntry.exists())
            entries.append(bundleEntry);
    }
    else
    {
        const QDir baseRoot = nx::branding::installationRoot() + "/client";
        entries = baseRoot.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs);
    }

    for (const auto& entry : std::as_const(entries))
    {
        nx::utils::SoftwareVersion version(entry.fileName());
        if (version.isNull())
            continue;

        const auto clientRoot = entry.absoluteFilePath();
        const auto binDir = QDir(clientRoot).absoluteFilePath(binDirSuffix());
        const auto clientBinary = QDir(binDir).absoluteFilePath(
            nx::branding::desktopClientLauncherName());

        if (!QFileInfo::exists(clientBinary))
        {
            NX_INFO(typeid(QnClientInstallationsManager), lit("Could not find client binary in %1").arg(clientBinary));
            NX_INFO(typeid(QnClientInstallationsManager), lit("Skip client root: %1").arg(clientRoot));
            continue;
        }

        result << clientRoot;
    }

    return result;
}

QFileInfo QnClientInstallationsManager::miniLauncher()
{
    const auto roots = clientInstallRoots();
    for (const QDir installRoot: roots)
    {
        const QDir binDir(installRoot.absoluteFilePath(binDirSuffix()));

        const QFileInfo minilauncherInfo(
            binDir.absoluteFilePath(nx::branding::minilauncherBinaryName()));

        if (!minilauncherInfo.exists())
            continue;

        return minilauncherInfo;
    }

    return QFileInfo();
}

QFileInfo QnClientInstallationsManager::appLauncher()
{
    const QDir applauncherDir(
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + lit("/../applauncher/")
        + nx::branding::customization());
    const QDir binDir(applauncherDir.absoluteFilePath(binDirSuffix()));
    return QFileInfo(binDir.absoluteFilePath(nx::branding::applauncherBinaryName()));
}
