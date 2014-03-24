////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "installation_manager.h"

#include <cstdlib>

#include <QtCore/QStandardPaths>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QRegExp>

#include <utils/common/log.h>

#include "version.h"


namespace {
    QRegExp versionDirMatch( "\\d+\\.\\d+.*" );

#ifdef Q_OS_LINUX
    QString installationPathPrefix = ".local/share";
#else
    QString installationPathPrefix = "AppData/Local";
#endif

    const QString INSTALLATION_DATA_FILE( "install.dat" );

    QnSoftwareVersion toMinorVersion(const QnSoftwareVersion &version) {
        return QnSoftwareVersion(version.major(), version.minor(), 0, 0);
    }

} // anonymous namespace


bool InstallationManager::AppData::exists() const {
    return !m_binaryPath.isEmpty() && QFile::exists(executableFilePath());
}

QnSoftwareVersion InstallationManager::AppData::version() const {
    return m_version;
}

QString InstallationManager::AppData::rootPath() const {
    return m_rootPath;
}

QString InstallationManager::AppData::binaryPath() const {
    return QFileInfo(executableFilePath()).absolutePath();
}

QString InstallationManager::AppData::executableFilePath() const {
    return m_binaryPath.isEmpty() ? QString() : m_rootPath + "/" + m_binaryPath;
}

QString InstallationManager::AppData::libraryPath() const {
    return m_libPath.isEmpty() ? QString() : m_rootPath + "/" + m_libPath;
}

bool InstallationManager::AppData::verifyInstallation() const
{
    NX_LOG(QString::fromLatin1("Entered VerifyInstallation"), cl_logDEBUG1);

    QMap<QString, qint64> fileSizeByEntry;
    QDir rootDir(m_rootPath);

    {
        QFile file(rootDir.absoluteFilePath(INSTALLATION_DATA_FILE));
        if (!file.open(QFile::ReadOnly))
            return false;

        QDataStream stream(&file);
        stream >> fileSizeByEntry;
        file.close();
    }

    if (fileSizeByEntry.isEmpty())
        return false;

    for (auto it = fileSizeByEntry.begin(); it != fileSizeByEntry.end(); ++it) {
        if (QFile(rootDir.absoluteFilePath(it.key())).size() != it.value())
            return false;
    }

    return true;
}


InstallationManager::InstallationManager(QObject *parent): QObject(parent)
{
    //TODO/IMPL disguise writable install directories for different modules

    //NOTE application path may be not-writable (actually it is always so if application running with no admistrator rights),
        //so selecting different path for new installations
//    QDir appDir( QCoreApplication::applicationDirPath() );
//    appDir.cdUp();
//    m_rootInstallDirectoryList.push_back( appDir.absolutePath() );

    m_defaultDirectoryForNewInstallations = defaultDirectoryForInstallations();
    if (!m_defaultDirectoryForNewInstallations.isEmpty())
        m_rootInstallDirectoryList.append(m_defaultDirectoryForNewInstallations);

    updateInstalledVersionsInformation();
}

InstallationManager::AppData InstallationManager::getAppData(const QString &rootPath) const
{
    QDir rootDir(rootPath);

    AppData appData;
    appData.m_rootPath = rootPath;

    QString binary = QN_CLIENT_EXECUTABLE_NAME;
    if (rootDir.exists(binary)) {
        appData.m_binaryPath = binary;
    } else {
        binary.prepend("bin/");
        if (rootDir.exists(binary))
            appData.m_binaryPath = binary;
    }

    QString lib = "lib";
    if (rootDir.exists(lib)) {
        appData.m_libPath = lib;
    } else {
        lib.prepend("../");
        if (rootDir.exists(lib))
            appData.m_libPath = lib;
    }

    return appData;
}

void InstallationManager::updateInstalledVersionsInformation()
{
    NX_LOG("Entered update installed versions", cl_logDEBUG1);
    QMap<QnSoftwareVersion, AppData> tempInstalledProductsByVersion;

    // detect current installation
    NX_LOG(QString::fromLatin1("Checking current version (%1)").arg(QN_APPLICATION_VERSION), cl_logDEBUG1);
    AppData current = getAppData(QCoreApplication::applicationDirPath());
    if (current.exists()) {
        current.m_version = toMinorVersion(QnSoftwareVersion(QN_APPLICATION_VERSION));
        tempInstalledProductsByVersion.insert(current.version(), current);
    } else {
        NX_LOG(QString::fromLatin1("Can't find client binary in %1").arg(current.rootPath()), cl_logWARNING);
    }

    // find other versions
    foreach (const QString &rootPath, m_rootInstallDirectoryList) {
        const QStringList &entries = QDir(rootPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        foreach (const QString &entry, entries) {
            //each entry - is a version
            if (!versionDirMatch.exactMatch(entry))
                continue;

            AppData appData = getAppData(rootPath + "/" + entry);

            if (!appData.exists())
                continue;

            appData.m_version = toMinorVersion(QnSoftwareVersion(entry));

            tempInstalledProductsByVersion.insert(appData.version(), appData);
            NX_LOG(QString::fromLatin1("Compatibility version %1 found").arg(entry), cl_logDEBUG1);
        }
    }

    {
        std::unique_lock<std::mutex> lk(m_mutex);
        m_installedProductsByVersion.swap(tempInstalledProductsByVersion);
    }

    createGhosts();
}

void InstallationManager::createGhosts()
{
    QString latestVersion = getMostRecentVersion();

    foreach (const QnSoftwareVersion &version, m_installedProductsByVersion.keys()) {
        if (version.toString(QnSoftwareVersion::MinorFormat) == latestVersion)
            continue;

        createGhostsForVersion(version);
    }
}

void InstallationManager::createGhostsForVersion(const QnSoftwareVersion &version)
{
    // Make version check. Applauncher doesn't need to affect versions >= 2.2 because they are able to communicate with applauncher
    if (QnSoftwareVersion(2, 1, 0, 0) < version)
        return;

    auto it = m_installedProductsByVersion.find(version);
    if (it == m_installedProductsByVersion.end())
        return;

    QDir targetDir(it.value().binaryPath());
    targetDir.cdUp();

    QSet<QString> entries = QSet<QString>::fromList(targetDir.entryList(QDir::Files));
    entries.remove(version.toString(QnSoftwareVersion::MinorFormat));

    foreach (const QnSoftwareVersion &ghostVersion, m_installedProductsByVersion.keys()) {
        if (ghostVersion == version)
            continue;

        QString ghostVersionString = ghostVersion.toString(QnSoftwareVersion::MinorFormat);

        if (entries.contains(ghostVersionString)) {
            entries.remove(ghostVersionString);
            continue;
        }

        QFile ghost(targetDir.absoluteFilePath(ghostVersionString));
        if (!ghost.exists()) {
            ghost.open(QFile::WriteOnly);
            ghost.close();
        }
    }

    // now check the rest entries and remove unnecessary dirs
    foreach (const QString &entry, entries) {
        if (versionDirMatch.exactMatch(entry))
            QFile::remove(targetDir.absoluteFilePath(entry));
    }
}

int InstallationManager::count() const
{
    std::unique_lock<std::mutex> lk( m_mutex );
    return m_installedProductsByVersion.size();
}

QString InstallationManager::getMostRecentVersion() const
{
    std::unique_lock<std::mutex> lk( m_mutex );
    return m_installedProductsByVersion.empty() ? QString() : m_installedProductsByVersion.lastKey().toString(QnSoftwareVersion::MinorFormat);
}

bool InstallationManager::isVersionInstalled( const QString& version ) const
{
    std::unique_lock<std::mutex> lk( m_mutex );

    auto iter = m_installedProductsByVersion.find(QnSoftwareVersion(version));
    if (iter == m_installedProductsByVersion.end())
        return false;

    lk.unlock();

    //checking that directory exists
    if (!iter.value().exists()) {
        m_installedProductsByVersion.erase(iter);
        return false;
    }

    return true;
}

bool InstallationManager::getInstalledVersionData(const QString &version, InstallationManager::AppData *const appData) const
{
    std::unique_lock<std::mutex> lk( m_mutex );

    auto it = m_installedProductsByVersion.find(QnSoftwareVersion(version));
    if (it == m_installedProductsByVersion.end())
        return false;

    *appData = it.value();
    return true;
}

QString InstallationManager::getRootInstallDirectory() const
{
    return m_defaultDirectoryForNewInstallations;
}

QString InstallationManager::getInstallDirForVersion( const QString& version ) const
{
    if( !isValidVersionName(version) ||
        m_defaultDirectoryForNewInstallations.isEmpty() )   //no writable installation path
    {
        return QString();
    }
    return m_defaultDirectoryForNewInstallations + "/" + version;
}

//!Returns previous error description
QString InstallationManager::errorString() const
{
    return m_errorString;
}

bool InstallationManager::isValidVersionName( const QString& version )
{
    return versionDirMatch.exactMatch(version);
}

QString InstallationManager::defaultDirectoryForInstallations()
{
    QString defaultDirectoryForNewInstallations = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!defaultDirectoryForNewInstallations.isEmpty())
        defaultDirectoryForNewInstallations += QString::fromLatin1("/%1/%2/compatibility/%3/").arg(installationPathPrefix, QN_ORGANIZATION_NAME, QN_CUSTOMIZATION_NAME);

    return defaultDirectoryForNewInstallations;
}

void InstallationManager::setErrorString( const QString& _errorString )
{
    m_errorString = _errorString;
}

static const QLatin1String MODULE_NAME("Client");
