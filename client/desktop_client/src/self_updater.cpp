#include "self_updater.h"

#include <QtCore/QDir>
#include <QtCore/QLockFile>
#include <QtCore/QStandardPaths>

#include <api/applauncher_api.h>

#include <client/client_app_info.h>
#include <client/client_module.h>
#include <client/client_startup_parameters.h>

#include <nx/vms/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/utils/platform/protocol_handler.h>

#include <utils/common/app_info.h>
#include <utils/applauncher_utils.h>
#include <utils/directory_backup.h>

namespace {

const int kUpdateLockTimeoutMs = 1000;

}

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
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    QString binaryPath = qApp->applicationFilePath();

#if defined(Q_OS_LINUX)
    binaryPath = qApp->applicationDirPath() + lit("/client");
#endif

    return nx::utils::registerSystemUriProtocolHandler(
        nx::vms::utils::AppInfo::nativeUriProtocol(),
        binaryPath,
        QnAppInfo::productNameLong(),
        nx::vms::utils::AppInfo::nativeUriProtocolDescription(),
        QnAppInfo::customizationName(),
        m_clientVersion);

#else
    return true;
#endif
}

bool SelfUpdater::updateApplauncher()
{
    /* Check if applauncher binary exists in our installation. */

    QString applauncherDirPath = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + lit("/../applauncher");
    QDir applauncherDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + lit("/../applauncher"));
    if (!QDir().mkpath(applauncherDirPath))
    {
        NX_LOG(lit("Cannot make folder for applaucher: %1").arg(applauncherDirPath), cl_logERROR);
        return false;
    }
    applauncherDirPath = applauncherDir.canonicalPath();

    QString lockFilePath = applauncherDirPath + lit("/update.lock");

    /* Lock updating file to make sure we will not try to update applauncher by several client instances. */
    QLockFile applauncherLock(lockFilePath);
    if (!applauncherLock.tryLock(kUpdateLockTimeoutMs))
    {
        NX_LOG(lit("Lock file is not available: %1").arg(lockFilePath), cl_logERROR);
        return false;
    }

    /* Check installed applauncher version. */
    QString versionFile = applauncherDirPath + lit("/version");
    auto applauncherVersion = getVersionFromFile(versionFile);
    if (m_clientVersion <= applauncherVersion)
    {
        NX_LOG(lit("Applauncher is up to date"), cl_logINFO);
        return true;
    }

    NX_LOG(lit("Applauncher is too old, updating from %1").arg(applauncherVersion.toString()), cl_logINFO);

    /* Check if no applauncher instance is running. If there is, try to kill it. */
    auto killApplauncherResult = applauncher::quitApplauncher();
    if (killApplauncherResult != applauncher::api::ResultType::ok &&                /*< Successfully killed. */
        killApplauncherResult != applauncher::api::ResultType::connectError)        /*< Not running. */
    {
        NX_LOG(lit("Could not kill running applauncher instance. Error code %1")
               .arg(QString::fromUtf8(applauncher::api::ResultType::toString(killApplauncherResult))), cl_logERROR);
        return false;
    }

    //TODO: #GDM appinfo?
    const QStringList kTargetFileFilters =
    {
        QLatin1String("*.dll"),
        QLatin1String("version"),
        QnClientAppInfo::applauncherBinaryName()
    };

    /* Move installed applaucher to backup folder. */
    QString backupDirPath = applauncherDirPath + lit("/backup");
    QnDirectoryBackup backup(applauncherDirPath, kTargetFileFilters, backupDirPath);
    if (!backup.backup(QnDirectoryBackup::Behavior::Move))
    {
        NX_LOG(lit("Could not backup to %1.").arg(backupDirPath), cl_logERROR);
        return false;
    }

    /* Copy our applauncher with all libs to destination folder. */
    QnDirectoryBackup updatedSource(qApp->applicationDirPath(), kTargetFileFilters, applauncherDirPath);
    bool success = updatedSource.backup();
    if (!success)
    {
        NX_LOG(lit("Failed to update Applauncher."), cl_logERROR);
        backup.restore();
    }

    /* Run updated (or restored) applauncher. */
    //TODO: #GDM this will not work on linux (LD_LIBRARY_PATH). We should start minilauncher instead.
    QString applauncherPath = applauncherDirPath + L'/' + QnClientAppInfo::applauncherBinaryName();

    //TODO: #GDM here will be passed arguments to just run, without compatibility client
    QStringList applauncherArguments;
    if (QFileInfo::exists(applauncherPath) && QProcess::startDetached(applauncherPath, applauncherArguments))
    {
        NX_LOG(lit("Applauncher process started successfully."), cl_logINFO);
    }
    else
    {
        NX_LOG(lit("Applauncher process could not be started %1.").arg(applauncherPath), cl_logERROR);
        success = false;
    }

    /* If we failed, return now. */
    if (!success)
    {
        return false;
    }

    /* Finally, is case o f success, save version to file. */
    if (!saveVersionToFile(versionFile, m_clientVersion))
    {
        NX_LOG(lit("Version could not be written to file: %1.").arg(versionFile), cl_logERROR);
        NX_LOG(lit("Failed to update Applauncher."), cl_logERROR);
        return false;
    }

    NX_LOG(lit("Applauncher updated successfully."), cl_logINFO);
    return true;
}

bool SelfUpdater::updateMinilauncher()
{
    return true;
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

    QByteArray data = version.toString().toUtf8();
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
    nx::utils::runAsAdministratorWithUAC(qApp->applicationFilePath(),
                                         QStringList()
                                         << QnStartupParameters::kSelfUpdateKey
                                         << QnStartupParameters::kAllowMultipleClientInstancesKey);
#elif defined(Q_OS_LINUX)

#elif defined(Q_OS_MAC)

#endif
}

} // namespace client
} // namespace vms
} // namespace nx
