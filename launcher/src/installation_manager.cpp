////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "installation_manager.h"

#include <cstdlib>

#include <QDir>

#include <utils/common/log.h>

#include "version.h"


//static const QLatin1String packageConfigurationFileName( "install.xml" );

QString ApplicationVersionData::toString() const
{
    return version+"-"+platform+"-"+arch;
}

ApplicationVersionData ApplicationVersionData::fromString( const QString& str )
{
    //TODO/IMPL
    return ApplicationVersionData();
}


InstallationManager::InstallationManager( QObject* const parent )
:
    QObject( parent )
{
}

int InstallationManager::count() const
{
    //TODO/IMPL
    return 0;
}

ApplicationVersionData InstallationManager::getMostRecentVersion() const
{
    //TODO/IMPL
    return ApplicationVersionData();
}

bool InstallationManager::isVersionInstalled( const ApplicationVersionData& version ) const
{
    //TODO/IMPL
    return false;
}

bool InstallationManager::getInstalledVersionData(
    const ApplicationVersionData& version,
    InstallationManager::AppData* const appData ) const
{
    //TODO/IMPL
    return false;
}

bool InstallationManager::install( const QString& packagePath )
{
    //TODO/IMPL
    return false;

#if 0
    //opening archive
    ZipArchiveReader archive;
    if( !archive.open( packagePath ) )
    {
        NX_LOG( QString::fromLatin1("Failed to open package %1. %2").arg(packagePath).arg(archive.errorString()) );
        setErrorString( archive.errorString() );
        return false;
    }

    //searching for install.xml
    std::auto_ptr<QIODevice> packageConfigurationFile( archive.readFile(packageConfigurationFileName) );
    if( !packageConfigurationFile.get() )
    {
        NX_LOG( QString::fromLatin1("Failed to install package %1. %2 was not found").arg(packagePath).arg(packageConfigurationFileName) );
        setErrorString( QString::fromLatin1("Configuration file %1 not found in package %2").arg(packageConfigurationFileName).arg(packagePath) );
        return false;
    }

    //unzipping it
    QByteArray packageConfigurationFileData = packageConfigurationFile->readAll();

    //TODO/IMPL parsing package configuration file
    PackageConfFileParsingHandler handler;

    //preparing directory for installation
    const QString& installDir = getRootInstallDirectory(handler.arch, handler.platform) + "/" + handler.version;
    if( !QDir().mkpath(installDir) )
    {
        NX_LOG( QString::fromLatin1("Failed to install package %1. Cannot create directory %2 installation").
            arg(packagePath).arg(installDir) );
        setErrorString( QString::fromLatin1("Cannot create directory %1").arg(installDir) );
        return false;
    }

    //unzipping whole archive to the prepared directory
    if( !archive.unzipArchiveToDir( installDir ) )
    {
        NX_LOG( QString::fromLatin1("Failed to install package %1. Cannot unzip archive to dir %2").
            arg(packagePath).arg(installDir) );
        setErrorString( QString::fromLatin1("Cannot create directory %1").arg(installDir) );
        return false;
    }

    NX_LOG( QString::fromLatin1("Successfully installed package %1 to directory %2").
        arg(packagePath).arg(installDir), cl_logDebug1 );
    return true;
#endif
}

//!Returns previous error description
QString InstallationManager::errorString() const
{
    return m_errorString;
}

void InstallationManager::setErrorString( const QString& _errorString )
{
    m_errorString = _errorString;
}

void InstallationManager::readInstalledVersions()
{
    const QString& productRootInstallDir = QString::fromLatin1("%1/%2").arg(getRootInstallDirectory()).arg(QLatin1String(QN_PRODUCT_NAME));
    const QStringList& entries = QDir(productRootInstallDir).entryList( QDir::Dirs );
    for( int i = 0; i < entries.size(); ++i )
    { 
        //each entry - is a version
        m_installedProductsByVersion.insert( std::make_pair( entries[i], AppData(QString::fromLatin1("%1/%2").arg(productRootInstallDir).arg(entries[i])) ) );
    }
}

QString InstallationManager::getRootInstallDirectory()
{
#ifdef _WIN32
#ifdef _WIN64
    const char* programFilesPath = getenv("ProgramW6432");
#elif defined(_WIN32)
    const char* programFilesPath = getenv("ProgramFiles");
#endif
    return QString::fromLatin1("%1/%2").arg(programFilesPath ? QLatin1String(programFilesPath) : QString()).arg(QLatin1String(QN_ORGANIZATION_NAME));
#else   //assuming unix
    return QString::fromLatin1("/opt/%1/").arg(QLatin1String(QN_ORGANIZATION_NAME));
#endif
}
