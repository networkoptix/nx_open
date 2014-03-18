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


#ifdef AK_DEBUG
static QRegExp versionDirMatch( ".*" );
#else
static QRegExp versionDirMatch( "\\d+\\.\\d+.*" );
#endif

#ifdef Q_OS_LINUX
static QString installationPathPrefix = ".local/share";
#else
static QString installationPathPrefix = "AppData/Local";
#endif

static const QString INSTALLATION_DATA_FILE( "install.dat" );


bool InstallationManager::AppData::exists() const {
    return !m_binaryPath.isEmpty() && QFile::exists(executablePath());
}

QString InstallationManager::AppData::version() const {
    return m_version;
}

QString InstallationManager::AppData::rootPath() const {
    return m_rootPath;
}

QString InstallationManager::AppData::executablePath() const {
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


InstallationManager::InstallationManager( QObject* const parent )
:
    QObject( parent )
{
    //TODO/IMPL disguise writable install directories for different modules

    //NOTE application path may be not-writable (actually it is always so if application running with no admistrator rights),
        //so selecting different path for new installations
//    QDir appDir( QCoreApplication::applicationDirPath() );
//    appDir.cdUp();
//    m_rootInstallDirectoryList.push_back( appDir.absolutePath() );

    m_defaultDirectoryForNewInstallations = QStandardPaths::writableLocation( QStandardPaths::HomeLocation );
    if( !m_defaultDirectoryForNewInstallations.isEmpty() )
    {
        m_defaultDirectoryForNewInstallations += QString::fromLatin1("/%1/%2/compatibility/%3/").arg(installationPathPrefix, QN_ORGANIZATION_NAME, QN_CUSTOMIZATION_NAME);
        m_rootInstallDirectoryList.push_back( m_defaultDirectoryForNewInstallations );
    }

    updateInstalledVersionsInformation();
    createLatestVersionGhosts();
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
    decltype(m_installedProductsByVersion) tempInstalledProductsByVersion;

    // detect current installation
    AppData current = getAppData(QCoreApplication::applicationDirPath());
    if (current.exists()) {
        QRegExp verRegExp("(\\d+\\.\\d+).*");
        if (verRegExp.exactMatch(QN_APPLICATION_VERSION))
            current.m_version = verRegExp.cap(1);
    }
    tempInstalledProductsByVersion.insert(current.version(), current);

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

            appData.m_version = entry;

            tempInstalledProductsByVersion.insert(entry, appData);
        }
    }

    std::unique_lock<std::mutex> lk(m_mutex);
    m_installedProductsByVersion.swap(tempInstalledProductsByVersion);
}

void InstallationManager::doCreateLatestVersionGhost(const AppData &appData, const QString &version)
{
    QDir binDir = QFileInfo(appData.executablePath()).absoluteDir();
    binDir.cdUp();
    doCreateLatestVersionGhost(binDir.absolutePath(), version);
}

void InstallationManager::doCreateLatestVersionGhost(const QString &path, const QString &version)
{
    QDir rootDir(path);

    bool skipDirCreation = false;

    /* The first - remove old possible ghosts */
    QStringList entries = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach (const QString &entry, entries) {
        if (entry == version) {
            skipDirCreation = true;
        } else {
            if (QDir(path + "/" + entry).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty())
                rootDir.rmdir(entry);
        }
    }

    /* The second - create new one */
    if (!skipDirCreation && !rootDir.exists(version))
        rootDir.mkdir(version);
}

void InstallationManager::createLatestVersionGhosts()
{
    QString version = getMostRecentVersion();

    QDir installationDir(m_defaultDirectoryForNewInstallations);
    QStringList entries = installationDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach (const QString &entry, entries) {
        if (entry == version)
            continue;
        else
            doCreateLatestVersionGhost(m_defaultDirectoryForNewInstallations + "/" + entry, version);
    }
}

void InstallationManager::createLatestVersionGhostForVersion(const QString &version)
{
    auto it = m_installedProductsByVersion.find(version);
    if (it == m_installedProductsByVersion.end())
        return;

    doCreateLatestVersionGhost(it.value(), getMostRecentVersion());
}

int InstallationManager::count() const
{
    std::unique_lock<std::mutex> lk( m_mutex );
    return m_installedProductsByVersion.size();
}

QString InstallationManager::getMostRecentVersion() const
{
    std::unique_lock<std::mutex> lk( m_mutex );
    //TODO/IMPL numeric sorting of versions is required
    return m_installedProductsByVersion.empty() ? QString() : m_installedProductsByVersion.lastKey();
}

bool InstallationManager::isVersionInstalled( const QString& version ) const
{
    std::unique_lock<std::mutex> lk( m_mutex );

    auto iter = m_installedProductsByVersion.find(version);
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

    auto it = m_installedProductsByVersion.find(version);
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

void InstallationManager::setErrorString( const QString& _errorString )
{
    m_errorString = _errorString;
}

static const QLatin1String MODULE_NAME("Client");
