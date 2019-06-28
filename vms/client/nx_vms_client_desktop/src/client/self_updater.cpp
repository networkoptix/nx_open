#include "self_updater.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QLockFile>
#include <QtCore/QProcess>
#include <QtCore/QStandardPaths>
#include <QtCore/QThread>

#include <client/client_app_info.h>
#include <client/client_module.h>
#include <client/client_startup_parameters.h>
#include <client/client_installations_manager.h>

#include <nx/vms/utils/app_info.h>
#include <nx/vms/utils/platform/protocol_handler.h>
#include <nx/vms/utils/desktop_file_linux.h>

#include <nx/utils/log/log.h>
#include <nx/utils/file_system.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/app_info.h>
#include <utils/common/app_info.h>
#include <utils/applauncher_utils.h>

namespace nx {
namespace vms {
namespace client {

namespace {

const int kUpdateLockTimeoutMs = 1000;
const QString kApplauncherDirectory = lit("applauncher");
const QString kApplauncherBackupDirectory = lit("applauncher-backup");

QDir applicationRootDir(const QDir& binDir)
{
    QDir dir(binDir);
    int stepsUp = QnClientAppInfo::binDirSuffix().split(L'/', QString::SkipEmptyParts).size();
    for (int i = 0; i < stepsUp; ++i)
        dir.cdUp();
    return dir;
}

bool runMinilaucherInternal(const QStringList& args)
{
    const QString minilauncherPath = qApp->applicationDirPath() + L'/'
        + QnClientAppInfo::minilauncherBinaryName();

    if (!QFileInfo::exists(minilauncherPath))
    {
        NX_ERROR(typeid(SelfUpdater), lm("Can not start Minilauncher: file %1 does not exist.").arg(minilauncherPath));
        return false;
    }
    else if (QProcess::startDetached(minilauncherPath, args)) /*< arguments are MUST here */
    {
        NX_INFO(typeid(SelfUpdater),
            lit("Minilauncher process started successfully."));
        return true;
    }

    NX_ERROR(typeid(SelfUpdater), lit("Minilauncher process could not be started %1.").arg(minilauncherPath));
    return false;
}

/**
 * Developer builds cannot be run without predefined environment on windows. We must not use these
 * builds as url handlers or copy applauncher from them.
 */
bool canBeRunFromTheCurrentDirectory()
{
    if (!nx::utils::AppInfo::isWindows())
        return true;

    const QString binaryPath = qApp->applicationDirPath();
    const QString qt5LibFile =
        #if defined(_DEBUG)
            "Qt5Cored.dll";
        #else
            "Qt5Core.dll";
        #endif

    // Detecting by checking if Qt5Core exists. It will give us 99% correctness.
    return QFileInfo::exists(QDir(binaryPath).absoluteFilePath(qt5LibFile));
}

} // namespace



SelfUpdater::SelfUpdater(const QnStartupParameters& startupParams) :
    m_clientVersion(QnAppInfo::applicationVersion())
{
    if (!startupParams.engineVersion.isEmpty())
        m_clientVersion = nx::utils::SoftwareVersion(startupParams.engineVersion);

    QMap<Operation, Result> results;
    // TODO: #3.1 #gdm #dklychkov Move function calls inside osCheck() method.
    results[Operation::RegisterUriHandler] = osCheck(Operation::RegisterUriHandler, registerUriHandler());
    results[Operation::UpdateApplauncher] = osCheck(Operation::UpdateApplauncher, updateApplauncher());
    if (nx::utils::AppInfo::isLinux() || nx::utils::AppInfo::isMacOsX())
    {
        results[Operation::UpdateMinilauncher] = Result::Success;
    }
    else
    {
        results[Operation::UpdateMinilauncher] =
            osCheck(Operation::UpdateMinilauncher, updateMinilauncher());
    }

    /* If we are already in self-update mode, just exit in any case. */
    if (startupParams.selfUpdateMode)
        return;

    if (std::any_of(results.cbegin(), results.cend(), [](Result value) { return value == Result::AdminRequired; } ))
        launchWithAdminPermissions();
}

bool SelfUpdater::registerUriHandler()
{
    // Mac version registers itself through the Info.plist.
    if (nx::utils::AppInfo::isMacOsX())
        return true;

    if (!canBeRunFromTheCurrentDirectory())
        return true;

    QString binaryPath = qApp->applicationFilePath();

    #if defined(Q_OS_LINUX)
        binaryPath = qApp->applicationDirPath() + lit("/client");
    #endif

    return nx::vms::utils::registerSystemUriProtocolHandler(
        nx::vms::utils::AppInfo::nativeUriProtocol(),
        binaryPath,
        QnAppInfo::productNameLong(),
        nx::vms::utils::AppInfo::nativeUriProtocolDescription(),
        QnAppInfo::customizationName(),
        m_clientVersion);
}

bool copyApplauncherInstance(const QDir& sourceDir, const QDir& targetDir)
{
    using namespace nx::utils::file_system;

    auto checkedCopy = [](const QString& src, const QString& dst)
        {
            auto result = copy(src, dst, OverwriteExisting);
            if (!result)
            {
                NX_ERROR(typeid(SelfUpdater), lit("Cannot copy %1 to %2. Code: %3")
                    .arg(src)
                    .arg(dst)
                    .arg(result.code));
                return false;
            }
            return true;
        };

    const bool windows = QnAppInfo::applicationPlatform() == lit("windows");

    const QDir sourceBinDir(sourceDir.absoluteFilePath(QnClientAppInfo::binDirSuffix()));
    const QDir targetBinDir(targetDir.absoluteFilePath(QnClientAppInfo::binDirSuffix()));

    if (!ensureDir(targetBinDir))
        return false;

    if (!checkedCopy(
        sourceBinDir.absoluteFilePath(QnClientAppInfo::applauncherBinaryName()),
        targetBinDir.absoluteFilePath(QnClientAppInfo::applauncherBinaryName())))
    {
        return false;
    }

    if (windows)
    {
        for (const auto& entry: sourceBinDir.entryList(QStringList{lit("*.dll")}, QDir::Files))
        {
            if (!checkedCopy(sourceBinDir.absoluteFilePath(entry), targetBinDir.absolutePath()))
                return false;
        }
    }
    else
    {
        const QDir sourceLibDir(sourceDir.absoluteFilePath(QnClientAppInfo::libDirSuffix()));
        const QDir targetLibDir(targetDir.absoluteFilePath(QnClientAppInfo::libDirSuffix()));

        if (!ensureDir(targetLibDir))
            return false;

        for (const auto& entry:
             sourceLibDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot))
        {
            if (!checkedCopy(sourceLibDir.absoluteFilePath(entry), targetLibDir.absolutePath()))
                return false;
        }
    }

    return true;
}

bool SelfUpdater::updateApplauncher()
{
    using namespace nx::vms::applauncher::api;
    using namespace nx::utils::file_system;

    if (!canBeRunFromTheCurrentDirectory())
        return true;

    /* Check if applauncher binary exists in our installation. */

    const QDir companyDataDir(
        QStandardPaths::writableLocation(QStandardPaths::DataLocation)
            + lit("/.."));
    if (!ensureDir(companyDataDir))
    {
        NX_ERROR(this, lit("Cannot create a folder for application data: %1")
               .arg(companyDataDir.absolutePath()));
        return false;
    }

    const QDir targetDir(companyDataDir.absoluteFilePath(kApplauncherDirectory)
        + L'/' + QnAppInfo::customizationName());
    const QDir backupDir(companyDataDir.absoluteFilePath(kApplauncherBackupDirectory)
        + L'-' + QnAppInfo::customizationName());

    const auto lockFilePath = companyDataDir.absoluteFilePath(lit("applauncher-update.lock"));

    /* Lock update file to make sure we will not try to update applauncher
       by several client instances. */
    QLockFile applauncherLock(lockFilePath);
    if (!applauncherLock.tryLock(kUpdateLockTimeoutMs))
    {
        NX_ERROR(this, lit("Lock file is not available: %1").arg(lockFilePath));
        return false;
    }

    /* Check installed applauncher version. */
    const auto versionFile = targetDir.absoluteFilePath(QnClientAppInfo::launcherVersionFile());
    const auto applauncherVersion = getVersionFromFile(versionFile);
    if (m_clientVersion <= applauncherVersion)
    {
        NX_INFO(this, lit("Applauncher is up to date"));

        if (backupDir.exists())
        {
            NX_INFO(this, lit("Removing the outdated backup %1").arg(backupDir.absolutePath()));
            QDir(backupDir).removeRecursively();
        }

        return true;
    }

    NX_INFO(this, lit("Updating applauncher from %1").arg(applauncherVersion.toString()));

    // Ensure applauncher will be started even if update failed.
    auto runApplauncherGuard = nx::utils::makeScopeGuard(
        [this]()
        {
            if (!runMinilaucher())
                NX_ERROR(this, lit("Could not run applauncher again!"));
        });


    static const int kKillApplauncherRetries = 10;
    static const int kRetryMs = 100;
    static const int kWaitProcessDeathMs = 100;

    /* Check if no applauncher instance is running. If there is, try to kill it. */
    ResultType killApplauncherResult = quitApplauncher();
    int retriesLeft = kKillApplauncherRetries;
    while (retriesLeft > 0 && killApplauncherResult != ResultType::connectError) // Not running.
    {
        QThread::msleep(kRetryMs);
        killApplauncherResult = quitApplauncher();
        --retriesLeft;
    }

    if (killApplauncherResult != ResultType::connectError)  // Applauncher is still running.
    {
        NX_ERROR(this, "Could not kill running applauncher instance. Error code %1",
            killApplauncherResult);
        return false;
    }

    // If we have tried at least one time, wait again.
    if (retriesLeft != kKillApplauncherRetries)
        QThread::msleep(kWaitProcessDeathMs);

    if (backupDir.exists())
    {
        NX_INFO(this, lit("Using the existing backup %1").arg(backupDir.absolutePath()));
        QDir(targetDir).removeRecursively();
    }
    else if (targetDir.exists())
    {
        if (!QDir().rename(targetDir.absolutePath(), backupDir.absolutePath()))
        {
            NX_ERROR(this, lit("Could not backup applauncher to %1").arg(backupDir.absolutePath()));
            // Continue without backup
        }
    }

    nx::utils::Guard guard;
    if (backupDir.exists())
    {
        guard = nx::utils::Guard(
            [this, backupDir, targetDir]()
            {
                QDir(targetDir).removeRecursively();
                if (!QDir().rename(backupDir.absolutePath(), targetDir.absolutePath()))
                {
                    NX_ERROR(this, lit("Could not restore applauncher from %1!")
                            .arg(backupDir.absolutePath()));
                }
            });
    }

    if (!ensureDir(targetDir))
    {
        NX_ERROR(this, lit("Cannot create a folder for applauncher: %1").arg(targetDir.absolutePath()));
        return false;
    }

    /* Copy our applauncher with all libs to destination folder. */
    const bool updateSuccess = copyApplauncherInstance(
        applicationRootDir(qApp->applicationDirPath()), targetDir);

    if (guard)
    {
        if (updateSuccess)
            guard.disarm();
        else
            guard.fire(); // Restore old applauncher.
    }

    /* If we failed, return now. */
    if (!updateSuccess)
        return false;

    if (!updateApplauncherDesktopIcon())
    {
        NX_ERROR(this, lit("Failed to update desktop icon."));
        return false;
    }

    /* Finally, is case of success, save version to file. */
    if (!saveVersionToFile(versionFile, m_clientVersion))
    {
        NX_ERROR(this, lit("Version could not be written to file: %1.").arg(versionFile));
        NX_ERROR(this, lit("Failed to update Applauncher."));
        return false;
    }

    QDir(backupDir).removeRecursively();

    NX_INFO(this, lit("Applauncher updated successfully."));
    return true;
}

bool SelfUpdater::updateMinilauncher()
{
    /* Do not try to update minilauncher when started from the default directory. */
    if (qApp->applicationDirPath().startsWith(QnClientAppInfo::installationRoot(), Qt::CaseInsensitive))
    {
        NX_INFO(this, lit("Minilauncher will not be updated."));
        return true;
    }

    const auto sourceMinilauncherPath = QDir(qApp->applicationDirPath()).absoluteFilePath(
        QnClientAppInfo::minilauncherBinaryName());
    if (!QFileInfo::exists(sourceMinilauncherPath))
    {
        NX_ERROR(this, lit("Source minilauncher could not be found at %1!")
            .arg(sourceMinilauncherPath));
        // Silently exiting because we still can't do anything.
        return true;
    }

    bool success = true;
    for (const QString& installRoot: QnClientInstallationsManager::clientInstallRoots())
        success &= updateMinilauncherInDir(installRoot, sourceMinilauncherPath);
    return success;
}

nx::utils::SoftwareVersion SelfUpdater::getVersionFromFile(const QString& filename) const
{
    QFile versionFile(filename);
    if (!versionFile.exists())
    {
        NX_ERROR(this, lit("Version file does not exist: %1").arg(filename));
        return nx::utils::SoftwareVersion();
    }

    if (!versionFile.open(QIODevice::ReadOnly))
    {
        NX_ERROR(this, lit("Version file could not be open for reading: %1").arg(filename));
        return nx::utils::SoftwareVersion();
    }

    return nx::utils::SoftwareVersion(versionFile.readLine());
}

bool SelfUpdater::saveVersionToFile(const QString& filename, const nx::utils::SoftwareVersion& version) const
{
    QFile versionFile(filename);
    if (!versionFile.open(QIODevice::WriteOnly))
    {
        NX_ERROR(this, lit("Version file could not be open for writing: %1").arg(filename));
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

bool SelfUpdater::runNewClient(const QStringList& args)
{
    return runMinilaucherInternal(args);
}

bool SelfUpdater::runMinilaucher()
{
    // We don't want another client instance here.
    const auto args = QStringList({ lit("--exec") });
    return runMinilaucherInternal(args);
}

bool SelfUpdater::updateMinilauncherInDir(const QDir& installRoot,
    const QString& sourceMinilauncherPath)
{
    if (isMinilaucherUpdated(installRoot))
        return true;

    if (QnAppInfo::applicationPlatform() == lit("windows"))
    {
        const auto applauncherPath =
            installRoot.absoluteFilePath(QnClientAppInfo::applauncherBinaryName());
        if (!QFileInfo::exists(applauncherPath))
        {
            NX_INFO(this, lit("Renaming applauncher..."));
            const auto sourceApplauncherPath =
                installRoot.absoluteFilePath(QnClientAppInfo::minilauncherBinaryName());
            if (!QFileInfo::exists(sourceApplauncherPath))
            {
                NX_ERROR(this, lit("Source applauncher could not be found at %1!")
                   .arg(sourceApplauncherPath));
                return false;
            }

            if (!QFile(sourceApplauncherPath).copy(applauncherPath))
            {
                NX_ERROR(this, lit("Could not copy applauncher from %1 to %2")
                       .arg(sourceApplauncherPath)
                       .arg(applauncherPath));
                return false;
            }
            NX_INFO(this, lit("Applauncher renamed successfully"));
        }
    }

    /* On linux (and mac?) applaucher binary should not be renamed, we only update shell script. */

    NX_INFO(this, lit("Updating minilauncher..."));
    const QDir binDir(installRoot.absoluteFilePath(QnClientAppInfo::binDirSuffix()));

    const QString minilauncherPath =
        binDir.absoluteFilePath(QnClientAppInfo::minilauncherBinaryName());
    const QString minilauncherBackupPath = minilauncherPath + lit(".bak");

    /* Existing minilauncher should be backed up. */
    if (QFileInfo::exists(minilauncherPath))
    {
        if (QFileInfo::exists(minilauncherBackupPath) && !QFile(minilauncherBackupPath).remove())
        {
            NX_ERROR(this, lit("Could not clean minilauncher backup %1")
               .arg(minilauncherBackupPath));
            return false;
        }

        if (!QFile(minilauncherPath).rename(minilauncherBackupPath))
        {
            NX_ERROR(this, lit("Could not backup minilauncher from %1 to %2")
                .arg(minilauncherPath, minilauncherBackupPath));
            return false;
        }
    }

    if (!QFile(sourceMinilauncherPath).copy(minilauncherPath))
    {
        NX_ERROR(this, lit("Could not copy minilauncher from %1 to %2")
            .arg(sourceMinilauncherPath, minilauncherPath));

        /* Silently trying to restore backup. */
        if (QFileInfo::exists(minilauncherBackupPath))
            QFile(minilauncherBackupPath).copy(minilauncherPath);

        return false;
    }

    /* Finally, is case of success, save version to file. */
    const auto versionFile = installRoot.absoluteFilePath(QnClientAppInfo::launcherVersionFile());
    if (!saveVersionToFile(versionFile, m_clientVersion))
    {
        NX_ERROR(this, lit("Version could not be written to file: %1.").arg(versionFile));
        NX_ERROR(this, lit("Failed to update Minilauncher."));
        return false;
    }

    NX_INFO(this, lit("Minilauncher updated successfully"));
    NX_ASSERT_HEAVY_CONDITION(isMinilaucherUpdated(installRoot));

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

        const auto applauncherPath =
            QDir(dataLocation).absoluteFilePath(lit("%1/applauncher/%2")
                .arg(QnAppInfo::organizationName(), QnAppInfo::customizationName()));

        auto iconName = AppInfo::iconFileName();

        const auto iconPath =
            QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(
                lit("../share/icons/%1").arg(iconName));

        if (QFile::exists(iconPath))
        {
            const auto targetIconPath =
                QDir(applauncherPath).absoluteFilePath(lit("share/icons/%1").arg(iconName));

            if (file_system::copy(iconPath, targetIconPath,
                file_system::OverwriteExisting | file_system::CreateTargetPath))
            {
                iconName = targetIconPath;
            }
        }

        const auto appsLocation =
            QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
        if (appsLocation.isEmpty())
            return false;

        const auto filePath = QDir(appsLocation).filePath(AppInfo::desktopFileName());

        const auto applauncherBinaryPath =
            QDir(applauncherPath).absoluteFilePath(lit("bin/%1")
                .arg(QnAppInfo::applauncherExecutableName()));

        if (!QFile::exists(applauncherBinaryPath))
            return false;

        return createDesktopFile(
            filePath,
            applauncherBinaryPath,
            QnAppInfo::productNameLong(),
            QnClientAppInfo::applicationDisplayName(),
            iconName,
            SoftwareVersion(QnAppInfo::applicationVersion()));
    #endif // defined(Q_OS_LINUX)

    return true;
}

} // namespace client
} // namespace vms
} // namespace nx
