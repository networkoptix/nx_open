#include "client_installations_manager.h"

#include <client/client_app_info.h>

#include <nx/utils/log/log.h>
#include <nx/utils/software_version.h>

QStringList QnClientInstallationsManager::clientInstallRoots()
{
    QStringList result;

    const QDir baseRoot(QnClientAppInfo::installationRoot() + lit("/client"));
    for (const auto& entry : baseRoot.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs))
    {
        nx::utils::SoftwareVersion version(entry.fileName());
        if (version.isNull())
            continue;

        const auto clientRoot = entry.absoluteFilePath();
        const auto binDir = QDir(clientRoot).absoluteFilePath(QnClientAppInfo::binDirSuffix());
        const auto clientBinary = QDir(binDir).absoluteFilePath(QnClientAppInfo::clientBinaryName());

        if (!QFileInfo::exists(clientBinary))
        {
            NX_LOG(lit("Could not find client binary in %1").arg(clientBinary), cl_logINFO);
            NX_LOG(lit("Skip client root: %1").arg(clientRoot), cl_logINFO);
            continue;
        }

        result << clientRoot;
    }

    return result;
}

QFileInfo QnClientInstallationsManager::miniLauncher()
{
    for (const QDir& installRoot: clientInstallRoots())
    {
        const QDir binDir(installRoot.absoluteFilePath(QnClientAppInfo::binDirSuffix()));

        const QFileInfo minilauncherInfo(
            binDir.absoluteFilePath(QnClientAppInfo::minilauncherBinaryName()));

        if (!minilauncherInfo.exists())
            continue;

        return minilauncherInfo;
    }

    return QFileInfo();
}
