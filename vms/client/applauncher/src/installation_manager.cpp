// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "installation_manager.h"

#include <chrono>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QRegularExpression>
#include <QtCore/QStandardPaths>

#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/utils/file_system.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qt_helpers.h>
#include <nx/zip/extractor.h>

#if defined(Q_OS_MACOS)
#include <nx/utils/platform/core_foundation_mac/cf_dictionary.h>
#include <nx/utils/platform/core_foundation_mac/cf_url.h>
#endif

using namespace std::chrono;

namespace {

static const QRegularExpression kVersionDirRegExp(
    QRegularExpression::anchoredPattern(R"(\d+\.\d+(?:\.\d+\.\d+){0,1})"));

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
QString kInstallationPathPrefix = ".local/share";
#else
QString kInstallationPathPrefix = "AppData/Local";
#endif

QString versionToString(const nx::utils::SoftwareVersion& version)
{
    if (version < nx::utils::SoftwareVersion(2, 2))
        return version.toString(nx::utils::SoftwareVersion::Format::minor);
    return version.toString();
}

QString applicationRootPath()
{
    const QDir dir(qApp->applicationDirPath());

    if (nx::build_info::isLinux())
        return dir.absoluteFilePath("..");

    return dir.absolutePath();
}

QString extractVersion([[maybe_unused]] const QString& fullPath)
{
    #if defined(Q_OS_MACOS)
        static const auto kShortVersionTag = "CFBundleShortVersionString";
        static const auto kBundleVersionTag = "CFBundleVersion";

        const auto url = cf::QnCFUrl::createFileUrl(fullPath);
        const cf::QnCFDictionary infoPlist(CFBundleCopyInfoDictionaryInDirectory(url.ref()));
        const auto shortVersion = infoPlist.getStringValue(kShortVersionTag);
        const auto bundleVersion = infoPlist.getStringValue(kBundleVersionTag);
        return QString("%1.%2").arg(shortVersion, bundleVersion);
    #else
        return QString();
    #endif
}


} // namespace

namespace nx::vms::applauncher {

QString InstallationManager::defaultDirectoryForInstallations()
{
    QString defaultDirectoryForNewInstallations = QStandardPaths::writableLocation(
        QStandardPaths::HomeLocation);
    if (!defaultDirectoryForNewInstallations.isEmpty())
    {
        defaultDirectoryForNewInstallations += QString("/%1/%2/client/%3/").arg(
            kInstallationPathPrefix,
            nx::branding::company(),
            nx::branding::customization());
    }

    return defaultDirectoryForNewInstallations;
}

void InstallationManager::removeInstallation(const nx::utils::SoftwareVersion& version)
{
    NX_INFO(this, "Removing version %1", version);

    QDir installationDir = installationDirForVersion(version);
    if (installationDir.exists())
        installationDir.removeRecursively();

    std::unique_lock<std::mutex> lock(m_mutex);
    m_installationByVersion.remove(version);
}

void InstallationManager::removeUnusedVersions()
{
    if (m_cleanupInProgress.exchange(true))
    {
        NX_INFO(this, "Previous cleanup has not finished yet. Skipping...");
        return;
    }

    NX_INFO(this, "Starting cleanup procedure...");

    QVector<nx::utils::SoftwareVersion> versionsToRemove;

    constexpr milliseconds kExpirationPeriod = 180 * 24h;
    constexpr milliseconds kCompatibleVersionExpirationPeriod = 30 * 24h;

    QMap<int, QnClientInstallationPtr> installationByProtocolVersion;

    const auto currentTime = duration_cast<milliseconds>(system_clock::now().time_since_epoch());

    nx::utils::SoftwareVersion newestVersion;

    {
        std::unique_lock<std::mutex> lock(m_mutex);

        for (const QnClientInstallationPtr& installation: m_installationByVersion)
        {
            if (installation->version() > newestVersion)
                newestVersion = installation->version();

            const milliseconds lastExecutionTime = installation->lastExecutionTime();

            if (installation->canBeRemoved()
                && lastExecutionTime < currentTime - kExpirationPeriod)
            {
                versionsToRemove.push_back(installation->version());
                continue;
            }

            const auto protocolVersion = installation->protocolVersion();
            if (protocolVersion == 0)
                continue;

            auto& previousInstallation = installationByProtocolVersion[protocolVersion];
            if (previousInstallation)
            {
                if (previousInstallation->canBeRemoved()
                    && previousInstallation->lastExecutionTime()
                        < currentTime - kCompatibleVersionExpirationPeriod)
                {
                    versionsToRemove.push_back(previousInstallation->version());
                }
            }
            previousInstallation = installation;
        }
    }

    versionsToRemove.removeOne(newestVersion);

    if (versionsToRemove.empty())
    {
        m_cleanupInProgress.store(false);
        NX_INFO(this, "Cleanup finished. Nothing was removed.");
        return;
    }

    m_cleanupFuture = std::async(std::launch::async,
        [this, versionsToRemove]()
        {
            for (const auto& version: versionsToRemove)
                removeInstallation(version);

            NX_INFO(this, "Cleanup finished. Removed %1 versions.", versionsToRemove.size());
            m_cleanupInProgress.store(false);
        });
}

InstallationManager::InstallationManager(QObject* parent):
    QObject(parent)
{
    //TODO/IMPL disguise writable install directories for different modules.

    m_installationsDir = QDir(defaultDirectoryForInstallations());
    updateInstalledVersionsInformation();
    removeUnusedVersions();
}

InstallationManager::~InstallationManager()
{
    // Scoped pointers cleanup.
}

void InstallationManager::updateInstalledVersionsInformation()
{
    NX_VERBOSE(this, "Entered update installed versions");
    QMap<nx::utils::SoftwareVersion, QnClientInstallationPtr> installations;

    // Detect current installation.
    const auto current =
        QnClientInstallation::installationForPath(applicationRootPath(), /*canBeRemoved*/ false);
    if (current)
    {
        if (current->version().isNull())
            current->setVersion(nx::utils::SoftwareVersion(nx::build_info::vmsVersion()));
        installations.insert(current->version(), current);
    }
    else
    {
        NX_DEBUG(this, "Can't find client binary in %1", QCoreApplication::applicationDirPath());
    }

    const auto fillInstallationFromDir =
        [&installations](const QString& path, QString version = QString())
        {
            auto clientPath = nx::build_info::isMacOsX()
                ? QDir(path).absoluteFilePath(nx::branding::vmsName() + ".app")
                : path;

            const QnClientInstallationPtr installation =
                QnClientInstallation::installationForPath(clientPath);

            if (installation.isNull())
                return;

            if (version.isNull())
                version = extractVersion(clientPath);

            installation->setVersion(nx::utils::SoftwareVersion(version));
            installations.insert(installation->version(), installation);
        };

    const auto fillInstallationsFromDir =
        [fillInstallationFromDir](const QDir& root)
        {
            const QStringList entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString& entry: entries)
            {
                if (kVersionDirRegExp.match(entry).hasMatch())
                {
                    const auto fullPath = root.absoluteFilePath(entry);
                    fillInstallationFromDir(fullPath, entry);
                }
            }
        };

    // Find other versions.
    fillInstallationsFromDir(m_installationsDir);

    if (nx::build_info::isMacOsX())
        fillInstallationFromDir(nx::branding::installationRoot());
    else
        fillInstallationsFromDir(nx::branding::installationRoot() + "/client");

    // Making a pretty log
    QStringList versions;
    for (const auto& record: installations.keys())
        versions << record.toString();

    if (!versions.empty())
        NX_DEBUG(this, "Compatibility versions: %1 found", versions);
    else
        NX_DEBUG(this, "No compatibility versions found");

    std::unique_lock<std::mutex> lk(m_mutex);
    m_installationByVersion = std::move(installations);
    lk.unlock();

    createGhosts();
}

void InstallationManager::createGhosts()
{
    nx::utils::SoftwareVersion latest = latestVersion();

    std::unique_lock<std::mutex> lk(m_mutex);
    QList<nx::utils::SoftwareVersion> versions = m_installationByVersion.keys();
    lk.unlock();

    for (const auto& version: versions)
    {
        if (version == latest)
            continue;

        createGhostsForVersion(version);
    }
}

void InstallationManager::createGhostsForVersion(const nx::utils::SoftwareVersion& version)
{
    // Applauncher doesn't need to affect versions >= 2.2 because they can communicate to applauncher.
    if (version >= nx::utils::SoftwareVersion(2, 2))
        return;

    QnClientInstallationPtr installation = installationForVersion(version);
    if (installation.isNull())
        return;

    QDir targetDir(installation->binaryPath());
    targetDir.cdUp();

    auto entries = nx::utils::toQSet(targetDir.entryList(QDir::Files));
    entries.remove(version.toString(nx::utils::SoftwareVersion::Format::minor));

    std::unique_lock<std::mutex> lk(m_mutex);
    QList<nx::utils::SoftwareVersion> versions = m_installationByVersion.keys();
    lk.unlock();

    for (const auto& ghostVersion: versions)
    {
        if (ghostVersion == version)
            continue;

        QString ghostVersionString =
            ghostVersion.toString(nx::utils::SoftwareVersion::Format::minor);

        if (entries.contains(ghostVersionString))
        {
            entries.remove(ghostVersionString);
            continue;
        }

        QFile ghost(targetDir.absoluteFilePath(ghostVersionString));
        if (!ghost.exists())
        {
            ghost.open(QFile::WriteOnly);
            ghost.close();
        }
    }

    // now check the rest entries and remove unnecessary dirs
    for (const auto& entry: entries)
    {
        if (kVersionDirRegExp.match(entry).hasMatch())
            QFile::remove(targetDir.absoluteFilePath(entry));
    }
}

int InstallationManager::count() const
{
    std::unique_lock<std::mutex> lk(m_mutex);
    return m_installationByVersion.size();
}

nx::utils::SoftwareVersion InstallationManager::latestVersion(int protocolVersion) const
{
    std::unique_lock<std::mutex> lk(m_mutex);
    if (m_installationByVersion.empty())
        return {};

    nx::utils::SoftwareVersion result;
    for (const auto& installation: m_installationByVersion)
    {
        // Map is sorted by version, so versions order strictly rises.
        if (protocolVersion == 0 || installation->protocolVersion() == protocolVersion)
            result = installation->version();
    }

    return result;
}

bool InstallationManager::isVersionInstalled(const nx::utils::SoftwareVersion& version, bool strict) const
{
    QnClientInstallationPtr installation = installationForVersion(version, strict);

    if (installation.isNull())
        return false;

    if (!installation->exists())
    {
        std::unique_lock<std::mutex> lk(m_mutex);
        m_installationByVersion.remove(installation->version());
        return false;
    }

    return true;
}

QList<nx::utils::SoftwareVersion> InstallationManager::installedVersions() const
{
    std::unique_lock<std::mutex> lk(m_mutex);
    return m_installationByVersion.keys();
}

nx::utils::SoftwareVersion InstallationManager::nearestInstalledVersion(
    const nx::utils::SoftwareVersion& version) const
{
    std::unique_lock<std::mutex> lk(m_mutex);

    if (m_installationByVersion.isEmpty())
        return nx::utils::SoftwareVersion();

    if (m_installationByVersion.contains(version))
        return version;

    auto it = m_installationByVersion.end();
    do
    {
        --it;

        nx::utils::SoftwareVersion v = it.key();
        // too early
        if (v.major > version.major)
            continue;

        // to last (
        if (v.major < version.major)
            break;

        // too early
        if (v.minor > version.minor)
            continue;

        // to last (
        if (v.minor < version.minor)
            break;

        // got it!
        return it.key();
    }
    while (it != m_installationByVersion.begin());

    return nx::utils::SoftwareVersion();
}

QnClientInstallationPtr InstallationManager::installationForVersion(
    const nx::utils::SoftwareVersion& version,
    bool strict) const
{
    std::unique_lock<std::mutex> lk(m_mutex);
    QnClientInstallationPtr installation = m_installationByVersion.value(version);
    lk.unlock();

    if (!installation)
    {
        //maybe installation is actually there but we do not know about it?
        //scanning dir once again
        const_cast<InstallationManager*>(this)->updateInstalledVersionsInformation();

        std::unique_lock<std::mutex> lk(m_mutex);
        installation = m_installationByVersion.value(version);
    }

    if (installation)
        return installation;

    if (strict)
        return QnClientInstallationPtr();

    nx::utils::SoftwareVersion ver = nearestInstalledVersion(
        nx::utils::SoftwareVersion(version.major, version.minor));
    if (ver.isNull())
        return QnClientInstallationPtr();

    lk.lock();
    return m_installationByVersion.value(ver);
}

QString InstallationManager::installationDirForVersion(
    const nx::utils::SoftwareVersion& version) const
{
    std::unique_lock<std::mutex> lk(m_mutex);
    return m_installationsDir.absoluteFilePath(versionToString(version));
}

// We create a dummy file to test if we have enough space for unpacked data.
bool dummySpaceCheck(const QDir& dir, qint64 size)
{
    QFile dummyFile(dir.filePath("dummytest.tmp"));
    if (!dummyFile.open(QFile::OpenModeFlag::WriteOnly))
        return false;

    const bool success = nx::utils::file_system::reserveSpace(dummyFile, size);

    dummyFile.close();
    dummyFile.remove();

    return success;
}

api::ResultType InstallationManager::installZip(
    const nx::utils::SoftwareVersion& version,
    const QString& fileName)
{
    // NOTE: This function can be started from a separate thread. Be safe with threading.
    NX_DEBUG(this, "Installing update %1 from %2", version, fileName);

    m_totalUnpackedSize = 0;

    QnClientInstallationPtr installation = installationForVersion(version, true);
    if (installation)
    {
        NX_INFO(this, "Version %1 is already installed", version);
        return api::ResultType::alreadyInstalled;
    }

    QDir targetDir(installationDirForVersion(version));

    if (targetDir.exists())
        targetDir.removeRecursively();

    if (!QDir().mkpath(targetDir.absolutePath()))
    {
        NX_ERROR(this, "Cannot create directory %1", targetDir.absolutePath());
        return api::ResultType::ioError;
    }

    auto extractor = QScopedPointer(new nx::zip::Extractor(fileName, targetDir));

    if (!dummySpaceCheck(targetDir, (qint64) extractor->estimateUnpackedSize()))
    {
        NX_ERROR(this, "Not enough space to install %1.", fileName);
        return api::ResultType::notEnoughSpace;
    }

    m_totalUnpackedSize = extractor->estimateUnpackedSize();

    {
        std::scoped_lock<std::mutex> lk(m_mutex);
        m_extractor.swap(extractor);
    }

    auto errorCode = m_extractor->extractZip();
    if (errorCode != nx::zip::Extractor::Ok)
    {
        NX_ERROR(this, "Cannot extract zip %1 to %2, errorCode = %3",
            fileName, targetDir.absolutePath(), (int)errorCode);
        return api::ResultType::ioError;
    }

    auto clientPath = nx::build_info::isMacOsX()
        ? targetDir.absoluteFilePath(nx::branding::vmsName() + ".app")
        : targetDir.absolutePath();

    installation = QnClientInstallation::installationForPath(clientPath);
    if (installation.isNull() || !installation->exists())
    {
        targetDir.removeRecursively();
        NX_ERROR(this, "Update package %1 (%2) is invalid", version, fileName);
        return api::ResultType::brokenPackage;
    }

    installation->setVersion(version);
    targetDir.remove("update.json");
    targetDir.remove("package.json");

    {
        std::scoped_lock<std::mutex> lk(m_mutex);
        m_installationByVersion.insert(installation->version(), installation);
    }

    removeUnusedVersions();
    createGhosts();

    NX_INFO(this, "Version %1 has been installed successfully to %2",
        version, targetDir.absolutePath());

    return api::ResultType::ok;
}

uint64_t InstallationManager::getBytesExtracted() const
{
    std::scoped_lock<std::mutex> lk(m_mutex);
    return m_extractor ? m_extractor->bytesExtracted() : 0;
}

uint64_t InstallationManager::getBytesTotal() const
{
    return m_totalUnpackedSize;
}

bool InstallationManager::isValidVersionName(const QString& version)
{
    return kVersionDirRegExp.match(version).hasMatch();
}

} // namespace nx::vms::applauncher
