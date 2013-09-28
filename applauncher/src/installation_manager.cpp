////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "installation_manager.h"

#include <cstdlib>

#include <QCoreApplication>
#include <QDir>
#include <QRegExp>

#include <utils/common/log.h>

#include "version.h"


//static const QLatin1String packageConfigurationFileName( "install.xml" );

InstallationManager::InstallationManager( QObject* const parent )
:
    QObject( parent )
{
    QDir appDir( QCoreApplication::applicationDirPath() );
    appDir.cdUp();
    m_rootInstallDirectory = appDir.absolutePath();

    readInstalledVersions();
}

int InstallationManager::count() const
{
    return m_installedProductsByVersion.size();
}

QString InstallationManager::getMostRecentVersion() const
{
    //TODO/IMPL numeric sorting of versions is required
    return m_installedProductsByVersion.empty() ? QString() : m_installedProductsByVersion.begin()->first;
}

bool InstallationManager::isVersionInstalled( const QString& version ) const
{
    return m_installedProductsByVersion.find(version) != m_installedProductsByVersion.end();
}

bool InstallationManager::getInstalledVersionData(
    const QString& version,
    InstallationManager::AppData* const appData ) const
{
    std::map<QString, AppData, std::greater<QString> >::const_iterator it = m_installedProductsByVersion.find(version);
    if( it == m_installedProductsByVersion.end() )
        return false;
    *appData = it->second;
    return true;
}

QString InstallationManager::getRootInstallDirectory() const
{
    return m_rootInstallDirectory;
}

QString InstallationManager::getInstallDirForVersion( const QString& version ) const
{
    if( !isValidVersionName(version) )
        return QString();
    return m_rootInstallDirectory + "/" + version;
}

//!Returns previous error description
QString InstallationManager::errorString() const
{
    return m_errorString;
}

#ifdef AK_DEBUG
static QRegExp versionDirMatch( ".*" );
#else
static QRegExp versionDirMatch( "\\d+\\.\\d+.*" );
#endif

bool InstallationManager::isValidVersionName( const QString& version )
{
    return versionDirMatch.exactMatch(version);
}

void InstallationManager::setErrorString( const QString& _errorString )
{
    m_errorString = _errorString;
}

static const QLatin1String MODULE_NAME("Client");

void InstallationManager::readInstalledVersions()
{
    const QStringList& entries = QDir(m_rootInstallDirectory).entryList( QDir::Dirs );
    for( int i = 0; i < entries.size(); ++i )
    {
        //each entry - is a version
        if( entries[i] == QLatin1String(".") || entries[i] == QLatin1String("..") || !versionDirMatch.exactMatch(entries[i]) )
            continue;
#ifdef AK_DEBUG
        if( entries[i] != QLatin1String("debug") )
            continue;
#endif
        m_installedProductsByVersion.insert( std::make_pair( entries[i], AppData(QString::fromLatin1("%1/%2").arg(m_rootInstallDirectory).arg(entries[i])) ) );
    }
}
