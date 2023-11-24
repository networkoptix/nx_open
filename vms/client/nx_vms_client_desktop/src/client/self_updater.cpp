// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "self_updater.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QLockFile>
#include <QtCore/QProcess>
#include <QtCore/QStandardPaths>
#include <QtCore/QThread>

#if defined(Q_OS_LINUX)
    #include <nx/vms/utils/desktop_file_linux.h>
#endif // defined(Q_OS_LINUX)

#include <client/client_installations_manager.h>
#include <client/client_module.h>
#include <client/client_startup_parameters.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/utils/file_system.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/utils/platform/protocol_handler.h>
#include <platform/platform_abstraction.h>
#include <platform/shortcuts/platform_shortcuts.h>
#include <utils/applauncher_utils.h>
#include <utils/platform/nov_association.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kUpdateLockTimeoutMs = 1000;
static const QString kApplauncher = "applauncher";
static const QString kApplauncherBackup = "applauncher-backup";
static const QString kQuickStartGuide = "quick_start_guide";
static const QString kMobileHelp = "mobile_help";
static const QString kDesktopFileName = nx::branding::installerName() + ".desktop";

QDir applicationRootDir(const QDir& binDir)
{
    QDir dir(binDir);
    const int stepsUp = QnClientInstallationsManager::binDirSuffix()
        .split('/', Qt::SkipEmptyParts).size();
    for (int i = 0; i < stepsUp; ++i)
        dir.cdUp();
    return dir;
}

QString componentDataDir(const QString& component)
{
    const auto dataLocation =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);

    return QDir(dataLocation).absoluteFilePath(QString("%1/%2/%3").arg(
        nx::branding::company(),
        component,
        nx::branding::customization()));
}

QString helpFilePath(const QString& fileName)
{
    const auto appDirPath = QDir(qApp->applicationDirPath());

    QStringList searchPathSuffixes{ "", "../", "../Resources/" };

    for(auto searchPathSuffix: searchPathSuffixes)
    {
        QString sourceFile = appDirPath.absoluteFilePath(searchPathSuffix + fileName);

        if (QFileInfo(sourceFile).exists())
            return sourceFile;
    }

    return QString();
}

bool hasHelpFilesFromInstaller()
{
    return build_info::isWindows();
}

QString desktopForAllUsersPathOnWindows()
{
    return QDir::fromNativeSeparators(QString::fromLocal8Bit(qgetenv("public"))) + "/Desktop";
}

QString applicationsForAllUsersPathOnWindows()
{
    return QDir::fromNativeSeparators(QString::fromLocal8Bit(qgetenv("allusersprofile")))
        + "/Microsoft/Windows/Start Menu/Programs/" + nx::branding::company();
}

bool runMinilaucherInternal(const QStringList& args)
{
    const QString minilauncherPath = qApp->applicationDirPath() + '/'
        + nx::branding::minilauncherBinaryName();

    if (!QFileInfo::exists(minilauncherPath))
    {
        if (nx::build_info::publicationType() != nx::build_info::PublicationType::local)
        {
            NX_ERROR(typeid(SelfUpdater), "Can not start Minilauncher: file %1 does not exist.",
                minilauncherPath);
        }
        return false;
    }
    else if (QProcess::startDetached(minilauncherPath, args)) /*< arguments are MUST here */
    {
        NX_INFO(typeid(SelfUpdater),
            lit("Minilauncher process started successfully."));
        return true;
    }

    NX_ERROR(typeid(SelfUpdater), "Minilauncher process could not be started %1.",
        minilauncherPath);
    return false;
}

bool isDeveloperBuildOnWindows()
{
    if (!build_info::isWindows())
        return false;

    const QString binaryPath = qApp->applicationDirPath();
    const QString qtLibFile =
        #if defined(_DEBUG)
            "Qt6Cored.dll";
        #else
            "Qt6Core.dll";
        #endif

    // Detecting by checking if Qt6Core exists. It will give us 99% correctness.
    return !QFileInfo::exists(QDir(binaryPath).absoluteFilePath(qtLibFile));
}

} // namespace


SelfUpdater::SelfUpdater(const QnStartupParameters& startupParams) :
    m_clientVersion(appContext()->version())
{
    if (!startupParams.engineVersion.isEmpty())
        m_clientVersion = nx::utils::SoftwareVersion(startupParams.engineVersion);

    bool hasAdminRights = startupParams.selfUpdateMode;
    bool probablyPermissionError = false;

    /**
     * Developer builds cannot be run without predefined environment on windows. We must not use
     * these builds as url handlers or copy applauncher from them.
     */
    if (!isDeveloperBuildOnWindows())
    {
        const auto uriHandlerUpdateResult = registerUriHandler();

        if (!uriHandlerUpdateResult.success)
        {
            if (hasAdminRights)
                NX_WARNING(this, "Unable to register URI handler.");
            else
                probablyPermissionError = true;
        }

        // TODO: #alevenkov Remove this action in ( 5.1 + 3 releases ).
        if (!registerNovFilesAssociation())
        {
            if (hasAdminRights)
                NX_WARNING(this, "Unable to register nov files association.");
            else
                probablyPermissionError = true;
        }

        updateApplauncher();

        bool updateHelpFilesNeeded = hasHelpFilesFromInstaller()
            ? uriHandlerUpdateResult.upgrade : true;

        if (updateHelpFilesNeeded)
        {
            std::vector<HelpFileDescription> helpFileDescriptions{
                {
                    .fileName = nx::branding::quickStartGuideFileName(),
                    .shortcutName = nx::branding::quickStartGuideShortcutName(),
                    .helpName = nx::branding::quickStartGuideDocumentName(),
                    .componentDataDirName = kQuickStartGuide
                },
            };

            if (nx::branding::isMobileClientEnabledInCustomization())
            {
                helpFileDescriptions.push_back(
                    {
                        .fileName = nx::branding::mobileHelpFileName(),
                        .shortcutName = nx::branding::mobileHelpShortcutName(),
                        .helpName = nx::branding::mobileHelpDocumentName(),
                        .componentDataDirName = kMobileHelp
                    }
                );
            }

            for (const auto& helpFileDescription: helpFileDescriptions)
            {
                if (!updateHelpFile(helpFileDescription))
                {
                    if (hasAdminRights)
                        NX_WARNING(this, "Unable to update %1.", helpFileDescription.helpName);
                    else
                        probablyPermissionError = true;
                }
            }
        }
    }

    // Update shortcuts for the current user / all users depending on the given access rights.
    if (nx::build_info::isWindows())
    {
        if (!updateMinilauncherOnWindows(hasAdminRights))
        {
            if (hasAdminRights)
                NX_WARNING(this, "Unable to update Minilauncher.");
            else
                probablyPermissionError = true;
        }

        if (probablyPermissionError)
            relaunchSelfUpdaterWithAdminPermissionsOnWindows();
    }
}

SelfUpdater::UriHandlerUpdateResult SelfUpdater::registerUriHandler()
{
    UriHandlerUpdateResult result;

    QString binaryPath = nx::build_info::isLinux()
        ? (qApp->applicationDirPath() + "/client")
        : qApp->applicationFilePath();

    auto registerHandlerResult = nx::vms::utils::registerSystemUriProtocolHandler(
        /*protocol*/ nx::branding::nativeUriProtocol(),
        /*applicationBinaryPath*/ binaryPath,
        /*applicationName*/ nx::branding::vmsName(),
        /*description*/ nx::branding::desktopClientDisplayName(),
        /*customization*/ nx::branding::customization(),
        /*version*/ m_clientVersion);

    if (!registerHandlerResult.success)
        return result;

    result.success = true;

    if (m_clientVersion > registerHandlerResult.previousVersion)
    {
        result.upgrade = true;

        NX_DEBUG(this, "Upgrading from %1 to %2 detected during registering URI handler.",
            registerHandlerResult.previousVersion,
            m_clientVersion);
    }

    return result;
}

bool SelfUpdater::registerNovFilesAssociation()
{
    const auto minilauncherPath = QDir(qApp->applicationDirPath()).absoluteFilePath(
        nx::branding::minilauncherBinaryName());
    if (!QFileInfo::exists(minilauncherPath))
    {
        NX_ERROR(this, "Source minilauncher could not be found at %1!", minilauncherPath);
        // Silently exiting because we still can't do anything.
        return true;
    }

    return nx::vms::utils::registerNovFilesAssociation(
        /*customizationId*/ nx::branding::customization(),
        /*applicationLauncherBinaryPath*/ minilauncherPath,
        /*applicationBinaryName*/ nx::branding::desktopClientBinaryName(),
        /*applicationName*/ nx::branding::vmsName()
    );
}

bool copyApplauncherInstance(const QDir& sourceDir, const QDir& targetDir)
{
    using namespace nx::utils::file_system;

    auto checkedCopy = [](const QString& src, const QString& dst)
        {
            auto result = copy(src, dst, OverwriteExisting);
            if (!result)
            {
                NX_ERROR(typeid(SelfUpdater), "Cannot copy %1 to %2. Code: %3", src, dst,
                    result.code);
                return false;
            }
            return true;
        };

    const bool windows = nx::build_info::isWindows();

    const QDir sourceBinDir(sourceDir.absoluteFilePath(
        QnClientInstallationsManager::binDirSuffix()));
    const QDir targetBinDir(targetDir.absoluteFilePath(
        QnClientInstallationsManager::binDirSuffix()));

    if (!ensureDir(targetBinDir))
        return false;

    if (!checkedCopy(
        sourceBinDir.absoluteFilePath(nx::branding::applauncherBinaryName()),
        targetBinDir.absoluteFilePath(nx::branding::applauncherBinaryName())))
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
        const QDir sourceLibDir(sourceDir.absoluteFilePath(
            QnClientInstallationsManager::libDirSuffix()));
        const QDir targetLibDir(targetDir.absoluteFilePath(
            QnClientInstallationsManager::libDirSuffix()));

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

void SelfUpdater::updateApplauncher()
{
    using namespace nx::vms::applauncher::api;
    using namespace nx::utils::file_system;

    /* Check if applauncher binary exists in our installation. */

    const QDir companyDataDir(
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
    if (!ensureDir(companyDataDir))
    {
        NX_ERROR(this, "Cannot create a folder for application data: %1",
            companyDataDir.absolutePath());
        return;
    }

    const QDir targetDir(componentDataDir(kApplauncher));
    const QDir backupDir(componentDataDir(kApplauncherBackup));

    const auto lockFilePath = companyDataDir.absoluteFilePath(lit("applauncher-update.lock"));

    /* Lock update file to make sure we will not try to update applauncher
       by several client instances. */
    QLockFile applauncherLock(lockFilePath);
    if (!applauncherLock.tryLock(kUpdateLockTimeoutMs))
    {
        NX_ERROR(this, "Lock file is not available: %1", lockFilePath);
        return;
    }

    /* Check installed applauncher version. */
    const auto versionFile = targetDir.absoluteFilePath(nx::branding::launcherVersionFile());
    const auto applauncherVersion = getVersionFromFile(versionFile);
    if (m_clientVersion <= applauncherVersion)
    {
        NX_INFO(this, lit("Applauncher is up to date"));

        if (backupDir.exists())
        {
            NX_INFO(this, "Removing the outdated backup %1", backupDir.absolutePath());
            QDir(backupDir).removeRecursively();
        }

        return;
    }

    NX_INFO(this, "Updating applauncher from %1", applauncherVersion.toString());

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
        return;
    }

    // If we have tried at least one time, wait again.
    if (retriesLeft != kKillApplauncherRetries)
        QThread::msleep(kWaitProcessDeathMs);

    if (backupDir.exists())
    {
        NX_INFO(this, "Using the existing backup %1", backupDir.absolutePath());
        QDir(targetDir).removeRecursively();
    }
    else if (targetDir.exists())
    {
        if (!QDir().rename(targetDir.absolutePath(), backupDir.absolutePath()))
        {
            NX_ERROR(this, "Could not backup applauncher to %1", backupDir.absolutePath());
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
                    NX_ERROR(this, "Could not restore applauncher from %1!",
                        backupDir.absolutePath());
                }
            });
    }

    if (!ensureDir(targetDir))
    {
        NX_ERROR(this, "Cannot create a folder for applauncher: %1", targetDir.absolutePath());
        return;
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
        return;

    if (!updateApplauncherDesktopIcon())
    {
        NX_ERROR(this, "Failed to update desktop icon.");
        return;
    }

    /* Finally, is case of success, save version to file. */
    if (!saveVersionToFile(versionFile, m_clientVersion))
    {
        NX_ERROR(this, "Version could not be written to file: %1.", versionFile);
        NX_ERROR(this, "Failed to update Applauncher.");
        return;
    }

    QDir(backupDir).removeRecursively();

    NX_INFO(this, lit("Applauncher updated successfully."));
}

bool SelfUpdater::updateHelpFile(const HelpFileDescription& helpDescription)
{
    using namespace nx::utils;
    using namespace nx::utils::file_system;

    NX_DEBUG(typeid(SelfUpdater), "Updating %1...", helpDescription.helpName);

    QString sourceFile = helpFilePath(helpDescription.fileName);
    if (sourceFile.isNull())
    {
        NX_WARNING(this, "No %1 file detected.", helpDescription.helpName);
        return true; //< We can't fix it, thus just ignore.
    }

    const QDir targetDir = hasHelpFilesFromInstaller()
        ? nx::branding::installationRoot()
        : componentDataDir(helpDescription.componentDataDirName);
    ensureDir(targetDir);
    const auto targetFile = targetDir.absoluteFilePath(helpDescription.fileName);

    bool helpAlreadyCopied = QFileInfo::exists(targetFile);
    // Create shortcut only on the first quick start guide document copying.
    bool shouldCreateDesktopShortcut = false;
    bool shouldCreateApplicationsShortcut = false;

    if (!helpAlreadyCopied)
    {
        file_system::Result copied = copy(sourceFile, targetFile, OverwriteExisting);

        if (!copied)
        {
            NX_ERROR(typeid(SelfUpdater), "Cannot copy %1 to %2. Code: %3",
                sourceFile,
                targetFile,
                copied.code);
            return false;
        }

        if (nx::build_info::isWindows())
        {
            shouldCreateDesktopShortcut = true;
            shouldCreateApplicationsShortcut = true;
        }

        if (nx::build_info::isMacOsX())
            shouldCreateDesktopShortcut = true;

        if (nx::build_info::isLinux())
            shouldCreateApplicationsShortcut = true;
    }
    else
    {
        NX_DEBUG(typeid(SelfUpdater), "%1 document already present on path %2.",
            helpDescription.helpName, targetFile);
    }

    auto createShortcutInDir =
        [&targetFile, &helpDescription](const QString& destinationDir)
        {
            bool success = qnPlatform->shortcuts()->createShortcut(
                targetFile,
                destinationDir,
                helpDescription.shortcutName);

            if (!success)
            {
                NX_ERROR(typeid(SelfUpdater), "Cannot create shortcut %1 in dir %2 to %3.",
                    helpDescription.shortcutName,
                    destinationDir,
                    targetFile);

                return false;
            }

            NX_DEBUG(typeid(SelfUpdater), "Shortcut %1 in dir %2 to %3 created successfully.",
                helpDescription.shortcutName,
                destinationDir,
                targetFile);

            return true;
        };

    if (shouldCreateDesktopShortcut)
    {
        QString desktopPath = hasHelpFilesFromInstaller()
            ? desktopForAllUsersPathOnWindows()
            : QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

        if (!createShortcutInDir(desktopPath))
            return false;
    }

    if (shouldCreateApplicationsShortcut)
    {
        QString applicationsPath = hasHelpFilesFromInstaller()
            ? applicationsForAllUsersPathOnWindows()
            : QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);

        if (!createShortcutInDir(applicationsPath))
            return false;
    }

    return true;
}

bool SelfUpdater::updateMinilauncherOnWindows(bool hasAdminRights)
{
    /* Do not try to update minilauncher when started from the default directory. */
    if (qApp->applicationDirPath().startsWith(nx::branding::installationRoot(),
        Qt::CaseInsensitive))
    {
        NX_INFO(this, lit("Minilauncher will not be updated."));
        return true;
    }

    const auto sourceMinilauncherPath = QDir(qApp->applicationDirPath()).absoluteFilePath(
        nx::branding::minilauncherBinaryName());
    if (!QFileInfo::exists(sourceMinilauncherPath))
    {
        NX_ERROR(this, "Source minilauncher could not be found at %1!", sourceMinilauncherPath);
        // Silently exiting because we still can't do anything.
        return true;
    }

    bool success = true;
    for (const QString& installRoot: QnClientInstallationsManager::clientInstallRoots())
        success &= updateMinilauncherOnWindowsInDir(installRoot, sourceMinilauncherPath);

    updateMinilauncherIconsOnWindows(hasAdminRights);

    return success;
}

nx::utils::SoftwareVersion SelfUpdater::getVersionFromFile(const QString& filename) const
{
    QFile versionFile(filename);
    if (!versionFile.exists())
    {
        NX_ERROR(this, "Version file does not exist: %1", filename);
        return nx::utils::SoftwareVersion();
    }

    if (!versionFile.open(QIODevice::ReadOnly))
    {
        NX_ERROR(this, "Version file could not be open for reading: %1", filename);
        return nx::utils::SoftwareVersion();
    }

    return nx::utils::SoftwareVersion(versionFile.readLine());
}

bool SelfUpdater::saveVersionToFile(const QString& filename,
    const nx::utils::SoftwareVersion& version) const
{
    QFile versionFile(filename);
    if (!versionFile.open(QIODevice::WriteOnly))
    {
        NX_ERROR(this, "Version file could not be open for writing: %1", filename);
        return false;
    }

    const QByteArray data = version.toString().toUtf8();
    return versionFile.write(data) == data.size();
}

void SelfUpdater::relaunchSelfUpdaterWithAdminPermissionsOnWindows()
{
    #if defined(Q_OS_WIN)
        /* Start another client instance with admin permissions if required. This instance performs
         * self update and exits. */
        nx::vms::utils::runAsAdministratorOnWindows(
            qApp->applicationFilePath(),
            QStringList()
                << QnStartupParameters::kSelfUpdateKey
                << QnStartupParameters::kAllowMultipleClientInstancesKey
                << "--log-level=info"
        );
    #endif
}

bool SelfUpdater::isMinilaucherUpdated(const QDir& installRoot) const
{
    const QDir binDir(installRoot.absoluteFilePath(QnClientInstallationsManager::binDirSuffix()));

    const QFileInfo minilauncherInfo(
        binDir.absoluteFilePath(nx::branding::minilauncherBinaryName()));
    if (!minilauncherInfo.exists())
        return false;

    const QFileInfo applauncherInfo(
        binDir.absoluteFilePath(nx::branding::applauncherBinaryName()));
    if (!binDir.exists(nx::branding::applauncherBinaryName()))
        return false; /*< That means there is an old applauncher only, it is named as minilauncher. */

    /* Check that files are different. This may be caused by unfinished update. */
    if (minilauncherInfo.size() == applauncherInfo.size())
        return false;

    /* Check installed applauncher version. */
    const auto versionFile =
        installRoot.absoluteFilePath(nx::branding::launcherVersionFile());
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

bool SelfUpdater::updateMinilauncherOnWindowsInDir(const QDir& installRoot,
    const QString& sourceMinilauncherPath)
{
    if (isMinilaucherUpdated(installRoot))
        return true;

    const auto applauncherPath =
        installRoot.absoluteFilePath(nx::branding::applauncherBinaryName());
    if (!QFileInfo::exists(applauncherPath))
    {
        NX_INFO(this, lit("Renaming applauncher..."));
        const auto sourceApplauncherPath =
            installRoot.absoluteFilePath(nx::branding::minilauncherBinaryName());
        if (!QFileInfo::exists(sourceApplauncherPath))
        {
            NX_ERROR(this, "Source applauncher could not be found at %1!", sourceApplauncherPath);
            return false;
        }

        if (!QFile(sourceApplauncherPath).copy(applauncherPath))
        {
            NX_ERROR(this, "Could not copy applauncher from %1 to %2", sourceApplauncherPath,
                applauncherPath);
            return false;
        }
        NX_INFO(this, lit("Applauncher renamed successfully"));
    }

    NX_INFO(this, lit("Updating minilauncher..."));
    const QDir binDir(installRoot.absoluteFilePath(QnClientInstallationsManager::binDirSuffix()));

    const QString minilauncherPath =
        binDir.absoluteFilePath(nx::branding::minilauncherBinaryName());
    const QString minilauncherBackupPath = minilauncherPath + lit(".bak");

    /* Existing minilauncher should be backed up. */
    if (QFileInfo::exists(minilauncherPath))
    {
        if (QFileInfo::exists(minilauncherBackupPath) && !QFile(minilauncherBackupPath).remove())
        {
            NX_ERROR(this, "Could not clean minilauncher backup %1", minilauncherBackupPath);
            return false;
        }

        if (!QFile(minilauncherPath).rename(minilauncherBackupPath))
        {
            NX_ERROR(this, "Could not backup minilauncher from %1 to %2", minilauncherPath,
                minilauncherBackupPath);
            return false;
        }
    }

    if (!QFile(sourceMinilauncherPath).copy(minilauncherPath))
    {
        NX_ERROR(this, "Could not copy minilauncher from %1 to %2", sourceMinilauncherPath,
            minilauncherPath);

        /* Silently trying to restore backup. */
        if (QFileInfo::exists(minilauncherBackupPath))
            QFile(minilauncherBackupPath).copy(minilauncherPath);

        return false;
    }

    /* Finally, is case of success, save version to file. */
    const auto versionFile = installRoot.absoluteFilePath(nx::branding::launcherVersionFile());
    if (!saveVersionToFile(versionFile, m_clientVersion))
    {
        NX_ERROR(this, "Version could not be written to file: %1.", versionFile);
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
    {
        const auto dataLocation =
            QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        if (dataLocation.isEmpty())
            return false;

        const auto applauncherPath = componentDataDir(kApplauncher);

        auto iconName = iconFileName();
        const auto iconPath =
            QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(
                QString("../share/icons/%1").arg(iconName));

        if (QFile::exists(iconPath))
        {
            const auto targetIconPath =
                QDir(applauncherPath).absoluteFilePath(QString("share/icons/%1").arg(iconName));

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

        const auto filePath = QDir(appsLocation).filePath(kDesktopFileName);

        const auto applauncherBinaryPath =
            QDir(applauncherPath).absoluteFilePath(QString("bin/%1")
                .arg(nx::branding::applauncherBinaryName()));

        if (!QFile::exists(applauncherBinaryPath))
            return false;

        return createDesktopFile(
            filePath,
            applauncherBinaryPath,
            nx::branding::vmsName(),
            nx::branding::desktopClientDisplayName(),
            iconName,
            appContext()->version());
    }
    #endif // defined(Q_OS_LINUX)

    return true;
}

void SelfUpdater::updateMinilauncherIconsOnWindows(bool hasAdminRights)
{
    // WIX installer creates a shortcut with icon placed in "%SystemRoot%\Installer\{GUID}\".
    // Instead of that we want to use an icon from our minilauncher binary, so it will be updated.

    NX_VERBOSE(this, "Updating desktop shortcuts");

    const QString desktopPath = hasAdminRights
        ? QDir::fromNativeSeparators(QString::fromLocal8Bit(qgetenv("public"))) + "/Desktop"
        : QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    const QString startMenuPath = nx::format(
        "%1/Microsoft/Windows/Start Menu/Programs/%2",
        QDir::fromNativeSeparators(QString::fromLocal8Bit(qgetenv("programdata"))),
        QCoreApplication::organizationName());

    NX_VERBOSE(this, "Desktop path: %1", desktopPath);

    const QStringList shortcutSourcePrefixes =
        QnClientInstallationsManager::clientInstallRoots();
    const QString shortcutSourceSuffix = nx::branding::minilauncherBinaryName();

    bool updated = false;

    const QFileInfoList desktopShortcuts = QDir(desktopPath).entryInfoList({"*.lnk"}, QDir::Files);
    const QFileInfoList startMenuShortcuts =
        QDir(startMenuPath).entryInfoList({"*.lnk"}, QDir::Files);

    for (const auto& entry: desktopShortcuts + startMenuShortcuts)
    {
        NX_VERBOSE(this, "Shortcut found: %1. Acquiring shortcut parameters.",
            entry.absoluteFilePath());

        // Acquire shortcut data.
        auto info = qnPlatform->shortcuts()->getShortcutInfo(
            entry.absolutePath(),
            entry.completeBaseName());

        // Check if the shortcut points to some minilauncher binary.
        const bool shortcutToLauncher = std::any_of(shortcutSourcePrefixes.begin(),
            shortcutSourcePrefixes.end(),
            [filePath = info.sourceFile](auto& prefix)
            {
                return filePath.startsWith(prefix, Qt::CaseInsensitive);
            })
            && info.sourceFile.endsWith(shortcutSourceSuffix, Qt::CaseInsensitive);

        // Additional check. We don't want to change VideoWall icons or user-chosen icons.
        if (shortcutToLauncher
            && (info.iconPath.contains("/Windows/Installer/", Qt::CaseInsensitive)
                || info.iconPath.contains("%SystemRoot%/Installer/", Qt::CaseInsensitive)))
        {
            NX_INFO(this, "Updating icon for shortcut at %1", entry.absoluteFilePath());
            NX_INFO(this, "Previous icon location: %1", info.iconPath);

            // Overwrite the shortcut.
            // This call clears shortcut icon, so the target binary icon will be used.
            bool success = qnPlatform->shortcuts()->createShortcut(
                info.sourceFile,
                entry.absolutePath(),
                entry.completeBaseName(),
                info.arguments);
            NX_INFO(this, "Success: %1", success);

            updated = true;
        }
        else
        {
            NX_VERBOSE(this, "Shortcut should not be updated");
        }
    }

    if (updated)
    {
        // Explorer caches program icons, so it doesn't need to parse binaries repeatedly.
        // Unfortunatelly, it doesn't make an icon obsolete when the original file is updated.
        // There are two ways to clear this cache: either delete iconcache database,
        // which may be located in at least three different locations (Win 7 / 8 / 10)
        // and may be inaccessible for writing while the Explorer is running,
        // or execute the "Internet Explorer Per-User Initialization Utility", which is not
        // well documented and whose arguments differs from version to version, but at least
        // which does not require Explorer restart to work.

        QProcess::startDetached("ie4uinit", {"-show"}); // Windows 10.
        QProcess::startDetached("ie4uinit", {"-ClearIconCache"}); // Other versions.
    }

    NX_VERBOSE(this, "Shortcut update finished");
}

} // namespace nx::vms::client::desktop
