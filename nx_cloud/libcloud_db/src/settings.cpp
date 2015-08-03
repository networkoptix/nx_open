/**********************************************************
* Jul 31, 2015
* a.kolesnikov
***********************************************************/

#include "settings.h"

#include <QLatin1String>
#include <QString>

#include <QtCore/QStandardPaths>

#include "version.h"


namespace
{
    static const QLatin1String LOG_LEVEL( "logLevel" );
#ifdef _DEBUG
    static const QLatin1String DEFAULT_LOG_LEVEL( "DEBUG" );
#else
    static const QLatin1String DEFAULT_LOG_LEVEL( "INFO" );
#endif

    static const QLatin1String ENDPOINTS_TO_LISTEN( "listenOn" );
    static const QLatin1String DEFAULT_ENDPOINTS_TO_LISTEN( ":3346" );

    static const QLatin1String DATA_DIR( "dataDir" );
    
    static const QLatin1String LOG_DIR( "logDir" );
}


namespace cdb
{


Settings::Settings()
:
#ifdef _WIN32
    m_settings(
        QSettings::SystemScope,
        QN_ORGANIZATION_NAME,
        QN_APPLICATION_NAME ),
#else
    m_settings(
        lit("/opt/%1/%2/etc/%2.conf").arg(VER_LINUX_ORGANIZATION_NAME).arg(VER_PRODUCTNAME_STR),
        QSettings::IniFormat ),
#endif
    m_showHelp( false )
{
}

bool Settings::showHelp() const
{
    return m_showHelp;
}

QString Settings::logLevel() const
{
    if( !m_logLevel.isEmpty() )
        return m_logLevel;
    return m_settings.value( LOG_LEVEL, DEFAULT_LOG_LEVEL ).toString();
}

std::list<SocketAddress> Settings::endpointsToListen() const
{
    const QStringList& httpAddrToListenStrList = m_settings.value(
        ENDPOINTS_TO_LISTEN,
        DEFAULT_ENDPOINTS_TO_LISTEN ).toString().split( ',' );
    std::list<SocketAddress> httpAddrToListenList( httpAddrToListenStrList.size() );
    std::transform(
        httpAddrToListenStrList.begin(),
        httpAddrToListenStrList.end(),
        std::back_inserter( httpAddrToListenList ),
        []( const QString& str ) { return SocketAddress( str ); } );

    return httpAddrToListenList;
}

QString Settings::dataDir() const
{
    const QString& dataDirFromSettings = m_settings.value( DATA_DIR ).toString();
    if( !dataDirFromSettings.isEmpty() )
        return dataDirFromSettings;

#ifdef Q_OS_LINUX
    QString defVarDirName = QString( "/opt/%1/%2/var" ).arg( VER_LINUX_ORGANIZATION_NAME ).arg( VER_PRODUCTNAME_STR );
    QString varDirName = m_settings->value( "varDir", defVarDirName ).toString();
    return varDirName;
#else
    const QStringList& dataDirList = QStandardPaths::standardLocations( QStandardPaths::DataLocation );
    return dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
}

QString Settings::logDir() const
{
    return m_settings.value(
        LOG_DIR,
        dataDir() + lit( "/log/" ) ).toString();
}

void Settings::parseArgs( int argc, char **argv )
{
    m_commandLineParser.parse( argc, argv, stderr );
}

void Settings::printCmdLineArgsHelp()
{
    //TODO #ak
}

void Settings::fillSupportedCmdParameters()
{
    m_commandLineParser.addParameter(
        &m_showHelp, "--help", NULL, "Show help message", false );
    m_commandLineParser.addParameter(
        &m_logLevel, "--log-level", NULL,
        lit( "Define log level. %1 by default" ).arg( DEFAULT_LOG_LEVEL ),
        DEFAULT_LOG_LEVEL );
}


}   //cdb
