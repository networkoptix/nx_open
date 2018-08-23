////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

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

static QRegExp kVersionDirRegExp("\\d+\\.\\d+(?:\\.\\d+\\.\\d+){0,1}");

#if defined(Q_OS_LINUX) || defined(Q_OS_MACX)
QString kInstallationPathPrefix = ".local/share";
#else
QString kInstallationPathPrefix = "AppData/Local";
#endif

QString versionToString(const nx::utils::SoftwareVersion& version)
{
    if (version < nx::utils::SoftwareVersion(2, 2))
        return version.toString(nx::utils::SoftwareVersion::MinorFormat);
    else
        return version.toString();
}

QString applicationRootPath()
{
    const QDir dir(qApp->applicationDirPath());

    if (QnAppInfo::applicationPlatform() == lit("linux"))
        return dir.absoluteFilePath("..");

    return dir.absolutePath();
}

#if defined(Q_OS_MACX)
QString extractVersion(const QString& fullPath)
{
    static const auto kShortVersionTag = lit("CFBundleShortVersionString");
    static const auto kBundleVersionTag = lit("CFBundleVersion");

    const auto url = cf::QnCFUrl::createFileUrl(fullPath);
    const cf::QnCFDictionary infoPlist(CFBundleCopyInfoDictionaryInDirectory(url.ref()));
    const auto shortVersion = infoPlist.getStringValue(kShortVersionTag);
    const auto bundleVersion = infoPlist.getStringValue(kBundleVersionTag);
    return lit("%1.%2").arg(shortVersion, bundleVersion);
}
#endif

} // namespace

QString InstallationManager::defaultDirectoryForInstallations()
{
    QString defaultDirectoryForNewInstallations = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!defaultDirectoryForNewInstallations.isEmpty())
    {
        defaultDirectoryForNewInstallations += QString::fromLatin1("/%1/%2/client/%3/").arg(
            kInstallationPathPrefix, QnAppInfo::organizationName(), QnAppInfo::customizationName());
    }

    return defaultDirectoryForNewInstallations;
}

void InstallationManager::removeInstallation(const SoftwareVersion& version)
{
    QDir installationDir = installationDirForVersion(version);
    if (installationDir.exists())
        installationDir.removeRecursively();

    std::unique_lock<std::mutex> lock(m_mutex);
    m_installationByVersion.remove(version);
}

InstallationManager::InstallationManager(QObject *parent):
    QObject(parent)
{
    //TODO/IMPL disguise writable install directories for different modules.

    m_installationsDir = QDir(defaultDirectoryForInstallations());
    updateInstalledVersionsInformation();
}

void InstallationManager::updateInstalledVersionsInformation()
{
    NX_LOG("Entered update installed versions", cl_logDEBUG1);

    QMap<SoftwareVersion, QnClientInstallationPtr> installations;

    // Detect current installation.
    NX_LOG(QString::fromLatin1("Checking current version (%1)").arg(
        QnAppInfo::applicationVersion()), cl_logDEBUG1);

    const auto current = QnClientInstallation::installationForPath(applicationRootPath());
    if (current)
    {
        current->setVersion(SoftwareVersion(QnAppInfo::applicationVersion()));
        current->setNeedsVerification(false);
        installations.insert(current->version(), current);
    }
    else
    {
        NX_LOG(QString::fromLatin1("Can't find client binary in %1").arg(
            QCoreApplication::applicationDirPath()), cl_logWARNING);
    }

    const auto fillInstallationFromDir =
        [this, &installations](const QString& path, bool verify, const QString& version)
        {
            const QnClientInstallationPtr installation =
                QnClientInstallation::installationForPath(path);

            if (installation.isNull())
                return;

            installation->setNeedsVerification(verify);

            if (verify && !installation->verify())
            {
                NX_LOG(QString::fromLatin1("Compatibility version %1 was not verified").arg(version),
                    cl_logDEBUG1);
                return;
            }

            installation->setVersion(SoftwareVersion(version));
            installations.insert(installation->version(), installation);

            NX_LOG(QString::fromLatin1("Compatibility version %1 found").arg(version), cl_logDEBUG1);
        };

    const auto fillInstallationsFromDir =
        [fillInstallationFromDir](const QDir& root, bool verify)
        {
            const QStringList entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &entry : entries)
            {
                if (kVersionDirRegExp.exactMatch(entry))
                {
                    const auto fullPath = root.absoluteFilePath(entry);
                    fillInstallationFromDir(fullPath, verify, entry);
                }
            }
        };

    // find other versions
#if defined(Q_OS_MACX)
    static const auto kPathPostfix = QString();
#else
    static const auto kPathPostfix = lit("/client/"); //< Default client install location.
#endif
    QString baseRoot = QnApplauncherAppInfo::installationRoot() + kPathPostfix;
    fillInstallationsFromDir(m_installationsDir, true);

    // We should verify downloaded installations only.
#if defined(Q_OS_MACX)
    const auto version = extractVersion(baseRoot + QnApplauncherAppInfo::bundleName());
    fillInstallationFromDir(baseRoot, false, version);
#else
    fillInstallationsFromDir(baseRoot, false); //< We should verify downloaded installations only.
#endif
    std::unique_lock<std::mutex> lk(m_mutex);
    m_installationByVersion = std::move(installations);
    lk.unlock();

    if (m_installationByVersion.empty())
        NX_LOG(lit("No client versions found"), cl_logWARNING);

    createGhosts();
}

void InstallationManager::createGhosts()
{
    SoftwareVersion latest = latestVersion();

    std::unique_lock<std::mutex> lk(m_mutex);
    QList<SoftwareVersion> versions = m_installationByVersion.keys();
    lk.unlock();

    foreach (const SoftwareVersion& version, versions) {
        if (version == latest)
            continue;

        createGhostsForVersion(version);
    }
}

void InstallationManager::createGhostsForVersion(const SoftwareVersion& version)
{
    // Applauncher doesn't need to affect versions >= 2.2 because they can communicate to applauncher.
    if (version >= SoftwareVersion(2, 2))
        return;

    QnClientInstallationPtr installation = installationForVersion(version);
    if (installation.isNull())
        return;

    QDir targetDir(installation->binaryPath());
    targetDir.cdUp();

    QSet<QString> entries = QSet<QString>::fromList(targetDir.entryList(QDir::Files));
    entries.remove(version.toString(SoftwareVersion::MinorFormat));

    std::unique_lock<std::mutex> lk(m_mutex);
    QList<SoftwareVersion> versions = m_installationByVersion.keys();
    lk.unlock();

    for (const auto& ghostVersion: versions)
    {
        if (ghostVersion == version)
            continue;

        QString ghostVersionString = ghostVersion.toString(SoftwareVersion::MinorFormat);

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

nx::utils::SoftwareVersion InstallationManager::latestVersion() const
{
    std::unique_lock<std::mutex> lk(m_mutex);
    return m_installationByVersion.empty() ? SoftwareVersion() : m_installationByVersion.lastKey();
}

bool InstallationManager::isVersionInstalled(const SoftwareVersion& version, bool strict) const
{
    QnClientInstallationPtr installation = installationForVersion(version, strict);

    if (installation.isNull())
        return false;

    if (!installation->exists()) {
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
    const SoftwareVersion& version) const
{
    std::unique_lock<std::mutex> lk(m_mutex);

    if (m_installationByVersion.isEmpty())
        return SoftwareVersion();

    if (m_installationByVersion.contains(version))
        return version;

    auto it = m_installationByVersion.end();
    do {
        --it;

        SoftwareVersion v = it.key();
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
    } while (it != m_installationByVersion.begin());

    return SoftwareVersion();
}

QnClientInstallationPtr InstallationManager::installationForVersion(
    const SoftwareVersion& version, bool strict) const
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

    SoftwareVersion ver = nearestInstalledVersion(SoftwareVersion(version.major(), version.minor()));
    if (ver.isNull())
        return QnClientInstallationPtr();

    lk.lock();
    return m_installationByVersion.value(ver);
}

QString InstallationManager::rootInstallationDirectory() const
{
    return m_installationsDir.absolutePath();
}

QString InstallationManager::installationDirForVersion(const SoftwareVersion& version) const
{
    std::unique_lock<std::mutex> lk(m_mutex);
    if (!m_installationsDir.exists())
        return QString();

    return m_installationsDir.absoluteFilePath(versionToString(version));
}

bool InstallationManager::installZip(const SoftwareVersion& version, const QString &fileName)
{
    NX_LOG(lit("InstallationManager: Installing update %1 from %2")
        .arg(version.toString()).arg(fileName), cl_logDEBUG1);

    QnClientInstallationPtr installation = installationForVersion(version, true);
    if (installation && installation->verify())
    {
        NX_LOG(lit("InstallationManager: Version %1 is already installed").arg(version.toString()),
            cl_logINFO);
        return true;
    }

    QDir targetDir(installationDirForVersion(version));

    if (targetDir.exists())
        targetDir.removeRecursively();

    if (!QDir().mkdir(targetDir.absolutePath()))
    {
        NX_LOG(lit("InstallationManager: Cannot create directory %1").arg(targetDir.absolutePath()),
            cl_logERROR);
        return false;
    }

    QnZipExtractor extractor(fileName, targetDir);
    const int errorCode = extractor.extractZip();
    if (errorCode != QnZipExtractor::Ok)
    {
        NX_LOG(lit("InstallationManager: Cannot extract zip %1 to %2, errorCode = %3")
            .arg(fileName).arg(targetDir.absolutePath()).arg(errorCode), cl_logERROR);
        return false;
    }

    installation = QnClientInstallation::installationForPath(targetDir.absolutePath());
    if (installation.isNull() || !installation->exists())
    {
        targetDir.removeRecursively();
        NX_LOG(lit("InstallationManager: Update package %1 (%2) is invalid")
            .arg(version.toString()).arg(fileName), cl_logERROR);
        return false;
    }

    installation->setVersion(version);
    targetDir.remove(lit("update.json"));

    if (!installation->createInstallationDat())
    {
        targetDir.removeRecursively();
        NX_LOG(lit("InstallationManager: Cannot create verification file for %1.")
            .arg(version.toString()).arg(fileName), cl_logERROR);
        return false;
    }

    std::unique_lock<std::mutex> lk(m_mutex);
    m_installationByVersion.insert(installation->version(), installation);
    lk.unlock();

    createGhosts();

    NX_LOG(lit("InstallationManager: Version %1 has been installed successfully to %2")
        .arg(version.toString()).arg(targetDir.absolutePath()), cl_logINFO);

    return true;
}

bool InstallationManager::isValidVersionName(const QString& version)
{
    return kVersionDirRegExp.exactMatch(version);
}
