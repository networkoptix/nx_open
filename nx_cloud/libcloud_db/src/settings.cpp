/**********************************************************
* Jul 31, 2015
* a.kolesnikov
***********************************************************/

#include "settings.h"

#include <thread>

#include <QtCore/QLatin1String>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>

#include "version.h"


namespace
{
    static const QLatin1String LOG_LEVEL( "logLevel" );
#ifdef _DEBUG
    static const QLatin1String DEFAULT_LOG_LEVEL( "DEBUG" );
#else
    static const QLatin1String DEFAULT_LOG_LEVEL( "INFO" );
#endif
    static const QLatin1String LOG_DIR( "logDir" );

    static const QLatin1String ENDPOINTS_TO_LISTEN( "listenOn" );
    static const QLatin1String DEFAULT_ENDPOINTS_TO_LISTEN( ":3346" );

    static const QLatin1String DATA_DIR( "dataDir" );

    //DB options
    static const QLatin1String DB_DRIVER_NAME( "db/driverName" );
    static const QLatin1String DEFAULT_DB_DRIVER_NAME( "QMYSQL" );

    static const QLatin1String DB_HOST_NAME( "db/hostName" );
    static const QLatin1String DEFAULT_DB_HOST_NAME( "127.0.0.1" );

    static const QLatin1String DB_PORT( "db/port" );
    static const int DEFAULT_DB_PORT = 3306;

    static const QLatin1String DB_DB_NAME( "db/dbName" );
    static const QLatin1String DEFAULT_DB_DB_NAME( "nx_cloud" );

    static const QLatin1String DB_USER_NAME( "db/userName" );
    static const QLatin1String DEFAULT_DB_USER_NAME( "root" );

    static const QLatin1String DB_PASSWORD( "db/password" );
    static const QLatin1String DEFAULT_DB_PASSWORD( "" );

    static const QLatin1String DB_CONNECT_OPTIONS( "db/connectOptions" );
    static const QLatin1String DEFAULT_DB_CONNECT_OPTIONS( "" );

    static const QLatin1String DB_MAX_CONNECTIONS( "db/maxConnections" );
    static const QLatin1String DEFAULT_DB_MAX_CONNECTIONS( "1" );

    static const QLatin1String CLOUD_DB_URL( "db/maxConnections" );
    static const QLatin1String DEFAULT_CLOUD_DB_URL( "http://nowhere.com:3346/" );
}


namespace nx {
namespace cdb {
namespace conf {


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

const Logging& Settings::logging() const
{
    return m_logging;
}

const db::ConnectionOptions& Settings::dbConnectionOptions() const
{
    return m_dbConnectionOptions;
}

const QString& Settings::cloudBackendUrl() const
{
    return m_cloudBackendUrl;
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

void Settings::load( int argc, char **argv )
{
    m_commandLineParser.parse( argc, argv, stderr );
    loadConfiguration();
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

void Settings::loadConfiguration()
{
    //log
    m_logging.logLevel = !m_logLevel.isEmpty()
        ? m_logLevel
        : m_settings.value( LOG_LEVEL, DEFAULT_LOG_LEVEL ).toString();
    m_logging.logDir = m_settings.value( LOG_DIR, dataDir() + lit( "/log/" ) ).toString();

    //DB
    m_dbConnectionOptions.driverName = m_settings.value( DB_DRIVER_NAME, DEFAULT_DB_DRIVER_NAME ).toString();
    m_dbConnectionOptions.hostName = m_settings.value( DB_HOST_NAME, DEFAULT_DB_HOST_NAME ).toString();
    m_dbConnectionOptions.port = m_settings.value( DB_PORT, DEFAULT_DB_PORT ).toInt();
    m_dbConnectionOptions.dbName = m_settings.value( DB_DB_NAME, DEFAULT_DB_DB_NAME ).toString();
    m_dbConnectionOptions.userName = m_settings.value( DB_USER_NAME, DEFAULT_DB_USER_NAME ).toString();
    m_dbConnectionOptions.password = m_settings.value( DB_PASSWORD, DEFAULT_DB_PASSWORD ).toString();
    m_dbConnectionOptions.connectOptions = m_settings.value( DB_CONNECT_OPTIONS, DEFAULT_DB_CONNECT_OPTIONS ).toString();
    m_dbConnectionOptions.maxConnectionCount = m_settings.value( DB_MAX_CONNECTIONS, DEFAULT_DB_MAX_CONNECTIONS ).toUInt();
    if( m_dbConnectionOptions.maxConnectionCount == 0 )
        m_dbConnectionOptions.maxConnectionCount = std::thread::hardware_concurrency();

    m_cloudBackendUrl = m_settings.value( CLOUD_DB_URL, DEFAULT_CLOUD_DB_URL ).toString();
}

}   //conf
}   //cdb
}   //nx
