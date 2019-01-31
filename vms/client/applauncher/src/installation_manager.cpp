#include "installation_manager.h"

#include <QtCore/QStandardPaths>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QRegExp>

#include <applauncher_app_info.h>

#include <nx/utils/log/log.h>

#include <utils/common/app_info.h>
#include <utils/update/zip_utils.h>

#if defined(Q_OS_MACX)
#include <nx/utils/platform/core_foundation_mac/cf_url.h>
#include <nx/utils/platform/core_foundation_mac/cf_dictionary.h>
#endif

namespace {

static QRegExp kVersionDirRegExp(R"(\d+\.\d+(?:\.\d+\.\d+){0,1})");

#if defined(Q_OS_LINUX) || defined(Q_OS_MACX)
QString kInstallationPathPrefix = ".local/share";
#else
QString kInstallationPathPrefix = "AppData/Local";
#endif

QString versionToString(const nx::utils::SoftwareVersion& version)
{
    if (version < nx::utils::SoftwareVersion(2, 2))
        return version.toString(nx::utils::SoftwareVersion::MinorFormat);
    return version.toString();
}

QString applicationRootPath()
{
    const QDir dir(qApp->applicationDirPath());

    if (QnAppInfo::applicationPlatform() == "linux")
        return dir.absoluteFilePath("..");

    return dir.absolutePath();
}

#if defined(Q_OS_MACX)
QString extractVersion(const QString& fullPath)
{
    static const auto kShortVersionTag = "CFBundleShortVersionString";
    static const auto kBundleVersionTag = "CFBundleVersion";

    const auto url = cf::QnCFUrl::createFileUrl(fullPath);
    const cf::QnCFDictionary infoPlist(CFBundleCopyInfoDictionaryInDirectory(url.ref()));
    const auto shortVersion = infoPlist.getStringValue(kShortVersionTag);
    const auto bundleVersion = infoPlist.getStringValue(kBundleVersionTag);
    return QString("%1.%2").arg(shortVersion, bundleVersion);
}
#endif

} // namespace

QString InstallationManager::defaultDirectoryForInstallations()
{
    QString defaultDirectoryForNewInstallations = QStandardPaths::writableLocation(
        QStandardPaths::HomeLocation);
    if (!defaultDirectoryForNewInstallations.isEmpty())
    {
        defaultDirectoryForNewInstallations += QString("/%1/%2/client/%3/").arg(
            kInstallationPathPrefix,
            QnAppInfo::organizationName(),
            QnAppInfo::customizationName());
    }

    return defaultDirectoryForNewInstallations;
}

void InstallationManager::removeInstallation(const nx::utils::SoftwareVersion& version)
{
    QDir installationDir = installationDirForVersion(version);
    if (installationDir.exists())
        installationDir.removeRecursively();

    std::unique_lock<std::mutex> lock(m_mutex);
    m_installationByVersion.remove(version);
}

InstallationManager::InstallationManager(QObject* parent):
    QObject(parent)
{
    //TODO/IMPL disguise writable install directories for different modules.

    m_installationsDir = QDir(defaultDirectoryForInstallations());
    updateInstalledVersionsInformation();
}

void InstallationManager::updateInstalledVersionsInformation()
{
    NX_DEBUG(this, "Entered update installed versions");

    QMap<nx::utils::SoftwareVersion, QnClientInstallationPtr> installations;

    // detect current installation
    NX_DEBUG(this, lm("Checking current version (%1)").arg(QnAppInfo::applicationVersion()));

    const auto current = QnClientInstallation::installationForPath(applicationRootPath());
    if (current)
    {
        if (current->version().isNull())
            current->setVersion(nx::utils::SoftwareVersion(QnAppInfo::applicationVersion()));
        installations.insert(current->version(), current);
    }
    else
    {
        NX_WARNING(this, lm("Can't find client binary in %1")
            .arg(QCoreApplication::applicationDirPath()));
    }

    const auto fillInstallationFromDir =
        [this, &installations](const QString& path, const QString& version)
	    {
	        const QnClientInstallationPtr installation =
	            QnClientInstallation::installationForPath(path);

	        if (installation.isNull())
	            return;

	        NX_DEBUG(this, lm("Compatibility version %1 was not verified").arg(version));
            installation->setVersion(nx::utils::SoftwareVersion(version));
	        installations.insert(installation->version(), installation);

	        NX_DEBUG(this, lm("Compatibility version %1 found").arg(version));
	    };

    const auto fillInstallationsFromDir =
        [fillInstallationFromDir](const QDir& root)
	    {
	        const QStringList entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
	        for (const QString& entry: entries)
	        {
	            if (kVersionDirRegExp.exactMatch(entry))
	            {
	                const auto fullPath = root.absoluteFilePath(entry);
	                fillInstallationFromDir(fullPath, entry);
	            }
	        }
	    };

    // find other versions
    #if defined(Q_OS_MACX)
    	static const auto kPathPostfix = QString();
    #else
	    static const auto kPathPostfix = "/client/"; /*< Default client install location. */
    #endif
    QString baseRoot = QnApplauncherAppInfo::installationRoot() + kPathPostfix;
    fillInstallationsFromDir(m_installationsDir);

    #if defined(Q_OS_MACX)
    	const auto version = extractVersion(baseRoot + QnApplauncherAppInfo::bundleName());
	    fillInstallationFromDir(baseRoot, version);
    #else
    	fillInstallationsFromDir(baseRoot);
    #endif
    std::unique_lock<std::mutex> lk(m_mutex);
    m_installationByVersion = std::move(installations);
    lk.unlock();

    if (m_installationByVersion.empty())
        NX_WARNING(this, lm("No client versions found"));

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

    QSet<QString> entries = QSet<QString>::fromList(targetDir.entryList(QDir::Files));
    entries.remove(version.toString(nx::utils::SoftwareVersion::MinorFormat));

    std::unique_lock<std::mutex> lk(m_mutex);
    QList<nx::utils::SoftwareVersion> versions = m_installationByVersion.keys();
    lk.unlock();

    for (const auto& ghostVersion: versions)
    {
        if (ghostVersion == version)
            continue;

        QString ghostVersionString = ghostVersion.toString(nx::utils::SoftwareVersion::MinorFormat);

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
        if (kVersionDirRegExp.exactMatch(entry))
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
        if (v.major() > version.major())
            continue;

        // to last (
        if (v.major() < version.major())
            break;

        // too early
        if (v.minor() > version.minor())
            continue;

        // to last (
        if (v.minor() < version.minor())
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
        nx::utils::SoftwareVersion(version.major(), version.minor()));
    if (ver.isNull())
        return QnClientInstallationPtr();

    lk.lock();
    return m_installationByVersion.value(ver);
}

QString InstallationManager::rootInstallationDirectory() const
{
    return m_installationsDir.absolutePath();
}

QString InstallationManager::installationDirForVersion(
    const nx::utils::SoftwareVersion& version) const
{
    std::unique_lock<std::mutex> lk(m_mutex);
    if (!m_installationsDir.exists())
        return QString();

    return m_installationsDir.absoluteFilePath(versionToString(version));
}

// We create a dummy file to test if we have enough space for unpacked data.
// This trick is the easiest cross-platform way of checking for space.
bool dummySpaceCheck(const QDir& dir, qint64 checkedSize, qint64* actuallyWritten = nullptr)
{
    if (actuallyWritten)
        *actuallyWritten = 0;
    QFile dummyFile(dir.filePath("dummytest.tmp"));
    if (!dummyFile.open(QFile::OpenModeFlag::WriteOnly))
        return false;

    // QFile::resize does not suit this test. We should actually write some data to make sure
    // we have this space available. I've tested QFile::resize with ext4 filesystem with 1GB
    // limit: it is completely OK to dummyFile.resize(2GB). But consequental writing / of 2GB
    // crearly fails on 1GB mark.
    bool success = true;
    qint64 bytesLeft = checkedSize;
    constexpr int kBlockSize = 515;
    std::vector<char> emptyData(kBlockSize, 0);
    while (bytesLeft > 0)
    {
        qint64 writeSize = qMin<qint64>(bytesLeft, kBlockSize);
        qint64 written = dummyFile.write(&emptyData.front(), writeSize);
        if (written < 0)
        {
            success = false;
            break;
        }
        bytesLeft -= written;
    }

    if (actuallyWritten)
        *actuallyWritten = checkedSize - bytesLeft;
    dummyFile.close();
    dummyFile.remove();
    return success;
}

InstallationManager::ResultType InstallationManager::installZip(
    const nx::utils::SoftwareVersion& version,
    const QString& fileName)
{
    NX_DEBUG(this, lm("InstallationManager: Installing update %1 from %2")
        .arg(version.toString()).arg(fileName));

    QnClientInstallationPtr installation = installationForVersion(version, true);
    if (installation)
    {
        NX_INFO(this, lm("InstallationManager: Version %1 is already installed")
            .arg(version.toString()));
        return ResultType::alreadyInstalled;
    }

    QDir targetDir(installationDirForVersion(version));

    if (targetDir.exists())
        targetDir.removeRecursively();

    if (!QDir().mkdir(targetDir.absolutePath()))
    {
        NX_ERROR(this, lm("InstallationManager: Cannot create directory %1")
            .arg(targetDir.absolutePath()));
        return ResultType::ioError;
    }

    QnZipExtractor extractor(fileName, targetDir);

    qint64 spaceAvailable = 0;
    if (!dummySpaceCheck(targetDir, extractor.estimateUnpackedSize(), &spaceAvailable))
    {
        NX_ERROR(this,
            lm("InstallationManager: Not enough space to install %1. Only %2 bytes are available")
            .args(fileName, QString::number(spaceAvailable)));
        return ResultType::notEnoughSpace;
    }

    auto errorCode = extractor.extractZip();
    if (errorCode != QnZipExtractor::Ok)
    {
        NX_ERROR(this, lm("InstallationManager: Cannot extract zip %1 to %2, errorCode = %3")
            .args(fileName, targetDir.absolutePath(), extractor.errorToString(errorCode)));
        return ResultType::ioError;
    }

    installation = QnClientInstallation::installationForPath(targetDir.absolutePath());
    if (installation.isNull() || !installation->exists())
    {
        targetDir.removeRecursively();
        NX_ERROR(this, lm("InstallationManager: Update package %1 (%2) is invalid")
            .args(version.toString(), fileName));
        return ResultType::brokenPackage;
    }

    installation->setVersion(version);
    targetDir.remove("update.json");

    std::unique_lock<std::mutex> lk(m_mutex);
    m_installationByVersion.insert(installation->version(), installation);
    lk.unlock();

    createGhosts();

    NX_INFO(this, lm("InstallationManager: Version %1 has been installed successfully to %2")
        .args(version.toString(), targetDir.absolutePath()));

    return ResultType::ok;
}

bool InstallationManager::isValidVersionName(const QString& version)
{
    return kVersionDirRegExp.exactMatch(version);
}
