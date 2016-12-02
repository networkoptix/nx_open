#include "self_updater.h"

#include <QtCore/QDir>
#include <QtCore/QLockFile>
#include <QtCore/QStandardPaths>
#include <QtCore/QCoreApplication>

#include <api/applauncher_api.h>

#include <client/client_app_info.h>
#include <client/client_module.h>
#include <client/client_startup_parameters.h>

#include <nx/vms/utils/app_info.h>
#include <nx/vms/utils/platform/protocol_handler.h>
#include <nx/vms/utils/desktop_file_linux.h>

#include <nx/utils/log/log.h>
#include <nx/utils/file_system.h>
#include <utils/common/app_info.h>
#include <utils/applauncher_utils.h>
#include <utils/directory_backup.h>

namespace {

const int kUpdateLockTimeoutMs = 1000;

typedef QSharedPointer<QnBaseDirectoryBackup> QnDirectoryBackupPtr;

QDir applicationRootDir(const QDir& binDir)
{
    QDir dir(binDir);
    int stepsUp = QnClientAppInfo::binDirSuffix().split(L'/', QString::SkipEmptyParts).size();
    for (int i = 0; i < stepsUp; ++i)
        dir.cdUp();
    return dir;
}

} // namespace

namespace nx {
namespace vms {
namespace client {

SelfUpdater::SelfUpdater(const QnStartupParameters& startupParams) :
    m_clientVersion(QnAppInfo::applicationVersion())
{
    if (!startupParams.engineVersion.isEmpty())
        m_clientVersion = nx::utils::SoftwareVersion(startupParams.engineVersion);

    QMap<Operation, Result> results;
    results[Operation::RegisterUriHandler] = osCheck(Operation::RegisterUriHandler, registerUriHandler());
    results[Operation::UpdateApplauncher] = osCheck(Operation::UpdateApplauncher, updateApplauncher());
    results[Operation::UpdateMinilauncher] = osCheck(Operation::UpdateMinilauncher, updateMinilauncher());

    /* If we are already in self-update mode, just exit in any case. */
    if (startupParams.selfUpdateMode)
        return;

    if (std::any_of(results.cbegin(), results.cend(), [](Result value) { return value == Result::AdminRequired; } ))
        launchWithAdminPermissions();
}

bool SelfUpdater::registerUriHandler()
{
    QString binaryPath = qApp->applicationFilePath();

    #if defined(Q_OS_LINUX)
        binaryPath = qApp->applicationDirPath() + lit("/client");
    #elif defined(Q_OS_MACX)
        binaryPath = lit("%1/%2").arg(qApp->applicationDirPath(),
            QnClientAppInfo::protocolHandlerBundleName());
    #endif

    return nx::vms::utils::registerSystemUriProtocolHandler(
        nx::vms::utils::AppInfo::nativeUriProtocol(),
        binaryPath,
        QnAppInfo::productNameLong(),
        QnClientAppInfo::protocolHandlerBundleId(),
        nx::vms::utils::AppInfo::nativeUriProtocolDescription(),
        QnAppInfo::customizationName(),
        m_clientVersion);
}

QnDirectoryBackupPtr copyApplauncherInstance(const QDir& from, const QDir& to)
{
    const auto backup = new QnMultipleDirectoriesBackup();

    // Copy launcher executable
    backup->addDirectoryBackup(QnDirectoryBackupPtr(new QnDirectoryBackup(
        from.absoluteFilePath(QnClientAppInfo::binDirSuffix()),
        QStringList{QnClientAppInfo::applauncherBinaryName(), lit("*.dll")},
        to.absoluteFilePath(QnClientAppInfo::binDirSuffix()))));

    // Copy libs
    if (QnAppInfo::applicationPlatform() != lit("windows"))
    {
        backup->addDirectoryBackup(QnDirectoryBackupPtr(new QnDirectoryRecursiveBackup(
            from.absoluteFilePath(QnClientAppInfo::libDirSuffix()),
            to.absoluteFilePath(QnClientAppInfo::libDirSuffix()))));
    }

    return QnDirectoryBackupPtr(backup);
}

bool SelfUpdater::updateApplauncher()
{
    using namespace applauncher::api;

    /* Check if applauncher binary exists in our installation. */

    QDir targetDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation)
        + lit("/../applauncher/")
        + QnAppInfo::customizationName());

    QString absolutePath = targetDir.absoluteFilePath(QnClientAppInfo::binDirSuffix());
    if (!QDir(absolutePath).mkpath(lit(".")))
    {
        NX_LOG(lit("Cannot create folder for applaucher: %1")
            .arg(absolutePath), cl_logERROR);
        return false;
    }

    const auto lockFilePath = targetDir.absoluteFilePath(lit("update.lock"));

    /* Lock update file to make sure we will not try to update applauncher
       by several client instances. */
    QLockFile applauncherLock(lockFilePath);
    if (!applauncherLock.tryLock(kUpdateLockTimeoutMs))
    {
        NX_LOG(lit("Lock file is not available: %1").arg(lockFilePath), cl_logERROR);
        return false;
    }

    /* Check installed applauncher version. */
    const auto versionFile = targetDir.absoluteFilePath(QnClientAppInfo::launcherVersionFile());
    const auto applauncherVersion = getVersionFromFile(versionFile);
    if (m_clientVersion <= applauncherVersion)
    {
        NX_LOG(lit("Applauncher is up to date"), cl_logINFO);
        return true;
    }

    NX_LOG(lit("Applauncher is too old, updating from %1")
        .arg(applauncherVersion.toString()), cl_logINFO);

    /* Check if no applauncher instance is running. If there is, try to kill it. */
    const auto killApplauncherResult = applauncher::quitApplauncher();
    if (killApplauncherResult != ResultType::ok &&          /*< Successfully killed. */
        killApplauncherResult != ResultType::connectError)  /*< Not running. */
    {
        NX_LOG(lit("Could not kill running applauncher instance. Error code %1")
            .arg(QString::fromUtf8(ResultType::toString(killApplauncherResult))), cl_logERROR);
        return false;
    }

    QDir backupDir(targetDir.absoluteFilePath(lit("backup")));
    const auto backup = copyApplauncherInstance(targetDir, backupDir);
    if (!backup->backup(QnDirectoryBackupBehavior::Move))
    {
        NX_LOG(lit("Could not backup to %1").arg(backupDir.absolutePath()), cl_logERROR);
        return false;
    }

    /* Copy our applauncher with all libs to destination folder. */
    auto updatedSource = copyApplauncherInstance(
        applicationRootDir(qApp->applicationDirPath()), targetDir);

    //TODO: #GDM may be rename class and its methods to something else?
    const bool updateSuccess = updatedSource->backup(QnDirectoryBackupBehavior::Copy);
    if (!updateSuccess)
    {
        NX_LOG(lit("Failed to update Applauncher."), cl_logERROR);
        backup->restore(QnDirectoryBackupBehavior::Copy);
    }

    /* Run newest applauncher via our own minilauncher. */
    if (!runMinilaucher())
        return false;

    if (!updateApplauncherDesktopIcon())
        return false;

    /* If we failed, return now. */
    if (!updateSuccess)
        return false;

    /* Finally, is case of success, save version to file. */
    if (!saveVersionToFile(versionFile, m_clientVersion))
    {
        NX_LOG(lit("Version could not be written to file: %1.").arg(versionFile), cl_logERROR);
        NX_LOG(lit("Failed to update Applauncher."), cl_logERROR);
        return false;
    }

    backupDir.removeRecursively();

    NX_LOG(lit("Applauncher updated successfully."), cl_logINFO);
    return true;
}

bool SelfUpdater::updateMinilauncher()
{
    /* Do not try to update minilauncher when started from the default directory. */
    if (qApp->applicationDirPath().startsWith(QnClientAppInfo::installationRoot(), Qt::CaseInsensitive))
    {
        NX_LOG(lit("Minilauncher will not be updated."), cl_logINFO);
        return true;
    }

    bool success = true;
    for (const QString& installRoot : getClientInstallRoots())
        success &= updateMinilauncherInDir(installRoot);
    return success;
}

nx::utils::SoftwareVersion SelfUpdater::getVersionFromFile(const QString& filename) const
{
    QFile versionFile(filename);
    if (!versionFile.exists())
    {
        NX_LOG(lit("Version file does not exist: %1").arg(filename), cl_logERROR);
        return nx::utils::SoftwareVersion();
    }

    if (!versionFile.open(QIODevice::ReadOnly))
    {
        NX_LOG(lit("Version file could not be open for reading: %1").arg(filename), cl_logERROR);
        return nx::utils::SoftwareVersion();
    }

    return nx::utils::SoftwareVersion(versionFile.readLine());
}

bool SelfUpdater::saveVersionToFile(const QString& filename, const nx::utils::SoftwareVersion& version) const
{
    QFile versionFile(filename);
    if (!versionFile.open(QIODevice::WriteOnly))
    {
        NX_LOG(lit("Version file could not be open for writing: %1").arg(filename), cl_logERROR);
        return false;
    }

    const QByteArray data = version.toString().toUtf8();
    return versionFile.write(data) == data.size();
}

SelfUpdater::Result SelfUpdater::osCheck(Operation operation, bool result)
{
    if (result)
        return Result::Success;

    #if defined(Q_OS_WIN)
        switch (operation)
        {
            case Operation::RegisterUriHandler:
            case Operation::UpdateMinilauncher:
                return Result::AdminRequired;
            case Operation::UpdateApplauncher:
                return Result::Failure;
            default:
                NX_ASSERT(false);
                break;
        }
        return Result::Failure;
    #elif defined(Q_OS_LINUX)
        switch (operation)
        {
            case Operation::UpdateMinilauncher:
                return Result::AdminRequired;
            case Operation::RegisterUriHandler:
            case Operation::UpdateApplauncher:
                return Result::Failure;
            default:
                NX_ASSERT(false);
                break;
        }
        return Result::Failure;
    #elif defined(Q_OS_MAC)
        return Result::Failure;
    #endif
}

void SelfUpdater::launchWithAdminPermissions()
{
    #if defined(Q_OS_WIN)
        /* Start another client instance with admin permissions if required. */
        nx::vms::utils::runAsAdministratorWithUAC(
            qApp->applicationFilePath(),
            QStringList()
            << QnStartupParameters::kSelfUpdateKey
            << QnStartupParameters::kAllowMultipleClientInstancesKey
            << lit("--log-level=info")
        );
    #elif defined(Q_OS_LINUX)

    #elif defined(Q_OS_MAC)

    #endif
}

bool SelfUpdater::isMinilaucherUpdated(const QDir& installRoot) const
{
    const QDir binDir(installRoot.absoluteFilePath(QnClientAppInfo::binDirSuffix()));

    const QFileInfo minilauncherInfo(
        binDir.absoluteFilePath(QnClientAppInfo::minilauncherBinaryName()));
    if (!minilauncherInfo.exists())
        return false;

    const QFileInfo applauncherInfo(
        binDir.absoluteFilePath(QnClientAppInfo::applauncherBinaryName()));
    if (!binDir.exists(QnClientAppInfo::applauncherBinaryName()))
        return false; /*< That means there is an old applauncher only, it is named as minilauncher. */

    /* Check that files are different. This may be caused by unfinished update. */
    if (minilauncherInfo.size() == applauncherInfo.size())
        return false;

    /* Check installed applauncher version. */
    const auto versionFile =
        installRoot.absoluteFilePath(QnClientAppInfo::launcherVersionFile());
    const auto minilauncherVersion = getVersionFromFile(versionFile);
    return (m_clientVersion <= minilauncherVersion);
}

bool SelfUpdater::runMinilaucher()
{
    const QString minilauncherPath = qApp->applicationDirPath() + L'/' + QnClientAppInfo::minilauncherBinaryName();
    const QStringList args = { lit("--exec") }; /*< We don't want another client instance here. */
    if (QFileInfo::exists(minilauncherPath) && QProcess::startDetached(minilauncherPath, args)) /*< arguments are MUST here */
    {
        NX_LOG(lit("Minilauncher process started successfully."), cl_logINFO);
        return true;
    }
    NX_LOG(lit("Minilauncher process could not be started %1.").arg(minilauncherPath), cl_logERROR);
    return false;
}

bool SelfUpdater::updateMinilauncherInDir(const QDir& installRoot)
{
    if (isMinilaucherUpdated(installRoot))
        return true;

    if (QnAppInfo::applicationPlatform() == lit("windows"))
    {
        const auto applauncherPath =
            installRoot.absoluteFilePath(QnClientAppInfo::applauncherBinaryName());
        if (!QFileInfo::exists(applauncherPath))
        {
            NX_LOG(lit("Renaming applauncher..."), cl_logINFO);
            const auto sourceApplauncherPath =
                installRoot.absoluteFilePath(QnClientAppInfo::minilauncherBinaryName());
            if (!QFileInfo::exists(sourceApplauncherPath))
            {
                NX_LOG(lit("Source applauncher could not be found at %1!")
                   .arg(sourceApplauncherPath), cl_logERROR);
                return false;
            }

            if (!QFile(sourceApplauncherPath).copy(applauncherPath))
            {
                NX_LOG(lit("Could not copy applauncher from %1 to %2")
                       .arg(sourceApplauncherPath)
                       .arg(applauncherPath),
                       cl_logERROR);
                return false;
            }
            NX_LOG(lit("Applauncher renamed successfully"), cl_logINFO);
        }
    }

    /* On linux (and mac?) applaucher binary should not be renamed, we only update shell script. */

    NX_LOG(lit("Updating minilauncher..."), cl_logINFO);
    const auto sourceMinilauncherPath = QDir(qApp->applicationDirPath()).absoluteFilePath(
        QnClientAppInfo::minilauncherBinaryName());
    if (!QFileInfo::exists(sourceMinilauncherPath))
    {
        NX_LOG(lit("Source minilauncher could not be found at %1!")
            .arg(sourceMinilauncherPath), cl_logERROR);
        return false;
    }

    const QDir binDir(installRoot.absoluteFilePath(QnClientAppInfo::binDirSuffix()));

    const QString minilauncherPath =
        binDir.absoluteFilePath(QnClientAppInfo::minilauncherBinaryName());
    const QString minilauncherBackupPath = minilauncherPath + lit(".bak");

    /* Existing minilauncher should be backed up. */
    if (QFileInfo::exists(minilauncherPath))
    {
        if (QFileInfo::exists(minilauncherBackupPath) && !QFile(minilauncherBackupPath).remove())
        {
            NX_LOG(lit("Could not clean minilauncher backup %1")
               .arg(minilauncherBackupPath), cl_logERROR);
            return false;
        }

        if (!QFile(minilauncherPath).rename(minilauncherBackupPath))
        {
            NX_LOG(lit("Could not backup minilauncher from %1 to %2")
                .arg(minilauncherPath, minilauncherBackupPath), cl_logERROR);
            return false;
        }
    }

    if (!QFile(sourceMinilauncherPath).copy(minilauncherPath))
    {
        NX_LOG(lit("Could not copy minilauncher from %1 to %2")
            .arg(sourceMinilauncherPath, minilauncherPath), cl_logERROR);

        /* Silently trying to restore backup. */
        if (QFileInfo::exists(minilauncherBackupPath))
            QFile(minilauncherBackupPath).copy(minilauncherPath);

        return false;
    }

    /* Finally, is case of success, save version to file. */
    const auto versionFile = installRoot.absoluteFilePath(QnClientAppInfo::launcherVersionFile());
    if (!saveVersionToFile(versionFile, m_clientVersion))
    {
        NX_LOG(lit("Version could not be written to file: %1.").arg(versionFile), cl_logERROR);
        NX_LOG(lit("Failed to update Minilauncher."), cl_logERROR);
        return false;
    }

    NX_LOG(lit("Minilauncher updated successfully"), cl_logINFO);
    NX_EXPECT(isMinilaucherUpdated(installRoot));

    return true;
}

bool SelfUpdater::updateApplauncherDesktopIcon()
{
    using namespace nx::utils;
    using namespace nx::vms::utils;

    #if defined(Q_OS_LINUX)
        const auto dataLocation =
            QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        if (dataLocation.isEmpty())
            return false;

        const auto iconsPath = QDir(QApplication::applicationDirPath()).absoluteFilePath(
            lit("../share/icons"));
        file_system::copy(iconsPath, dataLocation, file_system::OverwriteExisting);

        const auto appsLocation =
            QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
        if (appsLocation.isEmpty())
            return false;

        const auto filePath = QDir(appsLocation).filePath(AppInfo::iconFileName());

        const auto applauncherBinaryPath =
            QDir(dataLocation).absoluteFilePath(lit("%1/applauncher/%2/bin/%3")
                .arg(QnAppInfo::organizationName(),
                    QnAppInfo::customizationName(),
                    QnAppInfo::applauncherExecutableName()));

        if (!QFile::exists(applauncherBinaryPath))
            return false;

        return createDesktopFile(
            filePath,
            applauncherBinaryPath,
            QnAppInfo::productNameLong(),
            QnClientAppInfo::applicationDisplayName(),
            QnAppInfo::customizationName(),
            SoftwareVersion(QnAppInfo::engineVersion()));
    #endif // defined(Q_OS_LINUX)

    return true;
}

QStringList SelfUpdater::getClientInstallRoots() const
{
    QStringList result;

    const QDir baseRoot(QnClientAppInfo::installationRoot() + lit("/client"));
    for (const auto& entry: baseRoot.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs))
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

} // namespace client
} // namespace vms
} // namespace nx
