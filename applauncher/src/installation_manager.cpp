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

InstallationManager::InstallationManager( QObject* const parent )
:
    QObject( parent )
{
    //TODO/IMPL disguise writable install directories for different modules

    //NOTE application path may be not-writable (actually it is always so if application running with no admistrator rights),
        //so selecting different path for new installations
    QDir appDir( QCoreApplication::applicationDirPath() );
    appDir.cdUp();
    m_rootInstallDirectoryList.push_back( appDir.absolutePath() );
    m_defaultDirectoryForNewInstallations = QStandardPaths::writableLocation( QStandardPaths::HomeLocation );
    if( !m_defaultDirectoryForNewInstallations.isEmpty() )
    {
        m_defaultDirectoryForNewInstallations += QString::fromLatin1("/%1/%2/compatibility/%3/").arg(installationPathPrefix, QN_ORGANIZATION_NAME, QN_CUSTOMIZATION_NAME);
        m_rootInstallDirectoryList.push_back( m_defaultDirectoryForNewInstallations );
    }

    updateInstalledVersionsInformation();
    if( m_installedProductsByVersion.size() > 1 )
        createLatestVersionGhost();
}

void InstallationManager::updateInstalledVersionsInformation()
{
    decltype(m_installedProductsByVersion) tempInstalledProductsByVersion;

    for( const QString& rootDir: m_rootInstallDirectoryList  )
    {
        const QStringList& entries = QDir(rootDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for( int i = 0; i < entries.size(); ++i )
        {
            //each entry - is a version
            if( !versionDirMatch.exactMatch(entries[i]) )
                continue;

            // Skip empty dirs. Also skip created ghost dir.
            if( QDir(rootDir + QLatin1String("/") + entries[i]).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty() )
                continue;

            tempInstalledProductsByVersion.insert( std::make_pair(
                entries[i],
                AppData(
                    rootDir,
                    QString::fromLatin1("%1/%2").arg(rootDir).arg(entries[i])) ) );
        }
    }

    std::unique_lock<std::mutex> lk( m_mutex );
    m_installedProductsByVersion.swap( tempInstalledProductsByVersion );
}

void InstallationManager::createLatestVersionGhost()
{
    QString version = getMostRecentVersion();
    QDir installationDir(m_defaultDirectoryForNewInstallations);

    bool skipDirCreation = false;

    /* The first - remove old possible ghosts */
    QStringList entries = installationDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    qDebug() << entries;
    foreach( const QString &entry, entries ) {
        if( entry == version ) {
            skipDirCreation = true;
        } else {
            if (QDir(installationDir.absolutePath() + "/" + entry).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty())
                installationDir.rmdir(entry);
        }
    }

    /* The second - create new one */
    if( !skipDirCreation && !installationDir.exists(version) )
        installationDir.mkdir(version);
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
    return m_installedProductsByVersion.empty() ? QString() : m_installedProductsByVersion.begin()->first;
}

bool InstallationManager::isVersionInstalled( const QString& version ) const
{
    std::unique_lock<std::mutex> lk( m_mutex );

    auto iter = m_installedProductsByVersion.find(version);
    if( iter == m_installedProductsByVersion.end() )
        return false;

    const QString installationDirectory = iter->second.installationDirectory;

    lk.unlock();

    //checking that directory exists
    if( !QDir(installationDirectory).exists() )
    {
        m_installedProductsByVersion.erase( version );
        return false;
    }

    return true;
}

bool InstallationManager::getInstalledVersionData(
    const QString& version,
    InstallationManager::AppData* const appData ) const
{
    std::unique_lock<std::mutex> lk( m_mutex );

    std::map<QString, AppData, std::greater<QString> >::const_iterator it = m_installedProductsByVersion.find(version);
    if( it == m_installedProductsByVersion.end() )
        return false;
    *appData = it->second;
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
