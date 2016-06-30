#include "self_updater.h"

#include <QtCore/QDir>
#include <QtCore/QLockFile>

#include <api/applauncher_api.h>

#include <client/client_module.h>
#include <client/client_startup_parameters.h>

#include <nx/vms/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/utils/platform/protocol_handler.h>

#include <utils/common/app_info.h>
#include <utils/applauncher_utils.h>

namespace {

const int kUpdateLockTimeoutMs = 1000;


}

nx::vms::client::SelfUpdater::SelfUpdater(const QnStartupParameters& startupParams) :
    m_clientVersion(QnAppInfo::applicationVersion())
{
    if (!startupParams.engineVersion.isEmpty())
        m_clientVersion = nx::utils::SoftwareVersion(startupParams.engineVersion);

#ifdef Q_OS_WIN
    /* If handler must be updated AND client is not run under administrator, do it. */
    if (!registerUriHandler() && !startupParams.hasAdminPermissions)
    {
        /* Start another client instance with admin permissions if required. */
        nx::utils::runAsAdministratorWithUAC(qApp->applicationFilePath(),
                                             QStringList()
                                             << QnStartupParameters::kHasAdminPermissionsKey
                                             << QnStartupParameters::kAllowMultipleClientInstancesKey);
    }
#endif

    /* Updating applauncher only if we are not run under administrator to avoid collisions. */
    if (!startupParams.hasAdminPermissions)
        updateApplauncher();
}

bool nx::vms::client::SelfUpdater::registerUriHandler()
{
#ifdef Q_OS_WIN
    return nx::utils::registerSystemUriProtocolHandler(nx::vms::utils::AppInfo::nativeUriProtocol(),
                                                       qApp->applicationFilePath(),
                                                       nx::vms::utils::AppInfo::nativeUriProtocolDescription(),
                                                       m_clientVersion);
#else
    return true;
#endif
}

bool nx::vms::client::SelfUpdater::updateApplauncher()
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
        return false;
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

    /* Delete old backup. */
    QString backupDirPath = applauncherDirPath + lit("/backup");
    QDir backupDir(backupDirPath);
    if (!backupDir.removeRecursively())
    {
        NX_LOG(lit("Could not clear backup directory: %1").arg(backupDirPath), cl_logERROR);
        return false;
    }

    /* Move installed applaucher to backup folder. */
    {
        if (!applauncherDir.mkpath(backupDirPath))
            return false;
        QStringList oldFiles = applauncherDir.entryList(QDir::NoDotAndDotDot | QDir::Files);
        oldFiles.removeAll(lit("update.lock")); //TODO: #GDM duplicated string

        /* First, copy all files. */
        for (const QString& filename : oldFiles)
        {
            if (!QFile(filename).copy(backupDirPath + L'/' + filename))
                return false;
        }

        /* Only then delete existing. */
        for (const QString& filename : oldFiles)
        {
            if (!QFile::remove(filename))
                return false;
        }
    }

    /* Copy our applauncher with all libs to destination folder. */
    {
        //TODO: #GDM appinfo?
        QStringList filters = {
            QLatin1String("*.dll"),
            QLatin1String("applauncher.exe")
        };

        QString sourcePath = qApp->applicationDirPath();

        QStringList targetFiles = QDir(sourcePath).entryList(filters, QDir::NoDotAndDotDot | QDir::Files);
        for (const QString& filename : targetFiles)
        {
            QString sourceFilename = sourcePath + L'/' + filename;
            QString targetFilename = applauncherDirPath + L'/' + filename;
            if (!QFile(sourceFilename).copy(targetFilename))
            {
                NX_LOG(lit("Could not copy files: %1 to %2")
                       .arg(filename)
                       .arg(targetFilename), cl_logERROR);
                return false;
            }
        }
    }

    /* If failed, restore data from backup. */

    /* Run updated applauncher. */


    /* Finally, save version to file. */
    if (!saveVersionToFile(versionFile, m_clientVersion))
    {
        NX_LOG(lit("Version could not be written to file: %1").arg(versionFile), cl_logERROR);
        return false;
    }


    return true;
}

nx::utils::SoftwareVersion nx::vms::client::SelfUpdater::getVersionFromFile(const QString& filename) const
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

bool nx::vms::client::SelfUpdater::saveVersionToFile(const QString& filename, const nx::utils::SoftwareVersion& version) const
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
