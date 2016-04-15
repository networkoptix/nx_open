/**********************************************************
* Jul 31, 2015
* a.kolesnikov
***********************************************************/

#include "settings.h"

#include <thread>

#include <QtCore/QLatin1String>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>

#include <utils/serialization/lexical.h>
#include <utils/common/app_info.h>
#include <libcloud_db_app_info.h>

namespace
{
    //log settings
    const QLatin1String kLogLevel( "log/logLevel" );
#ifdef _DEBUG
    const QLatin1String kDefaultLogLevel( "DEBUG" );
#else
    const QLatin1String kDefaultLogLevel( "INFO" );
#endif
    const QLatin1String kLogDir( "log/logDir" );

    const QLatin1String kEndpointsToListen( "listenOn" );
    const QLatin1String kDefaultEndpointsToListen( "0.0.0.0:3346" );

    const QLatin1String kDataDir( "dataDir" );

    //DB options
    const QLatin1String kDbDriverName( "db/driverName" );
    const QLatin1String kDefaultDbDriverName( "QMYSQL" );

    const QLatin1String kDbHostName( "db/hostName" );
    const QLatin1String kDefaultDbHostName( "127.0.0.1" );

    const QLatin1String kDbPort( "db/port" );
    const int kDefaultDbPort = 3306;

    const QLatin1String kDbName( "db/name" );
    const QLatin1String kDefaultDbName( "nx_cloud" );

    const QLatin1String kDbUserName( "db/userName" );
    const QLatin1String kDefaultDbUserName( "root" );

    const QLatin1String kDbPassword( "db/password" );
    const QLatin1String kDefaultDbPassword( "" );

    const QLatin1String kDbConnectOptions( "db/connectOptions" );
    const QLatin1String kDefaultDbConnectOptions( "" );

    const QLatin1String kDbMaxConnections( "db/maxConnections" );
    const QLatin1String kDefaultDbMaxConnections( "1" );

    const QLatin1String kCloudDbUrl( "cloudDbUrl" );
    const QLatin1String kDefaultCloudDbUrl( "http://nowhere.com:3346/" );

    const QLatin1String kChangeUser( "changeUser" );

    //notification settings
    const QLatin1String kNotificationEnabled("notification/enabled");
    const bool kDefaultNotificationEnabled = true;

    const QLatin1String kPasswordResetCodeExpirationTimeout("accountManager/passwordResetCodeExpirationTimeoutSec");
    const std::chrono::seconds kDefaultPasswordResetCodeExpirationTimeout = std::chrono::hours(24);

    const QLatin1String kReportRemovedSystemPeriodSec("systemManager/reportRemovedSystemPeriodSec");
    const std::chrono::seconds kDefaultReportRemovedSystemPeriodSec = std::chrono::hours(30 * 24);  //a month

    const QLatin1String kNotActivatedSystemLivePeriodSec("systemManager/notActivatedSystemLivePeriodSec");
    const std::chrono::seconds kDefaultNotActivatedSystemLivePeriodSec = std::chrono::hours(30 * 24);  //a month

    const QLatin1String kDropExpiredSystemsPeriodSec("systemManager/dropExpiredSystemsPeriodSec");
    const std::chrono::seconds kDefaultDropExpiredSystemsPeriodSec = std::chrono::hours(12);

    const QLatin1String kControlSystemStatusByDb("systemManager/controlSystemStatusByDb");
    const bool kDefaultControlSystemStatusByDb = false;

    //auth settings
    const QString kModuleName = lit("cloud_db");

    const QLatin1String kAuthXmlPath("auth/rulesXmlPath");
    const QLatin1String kDefaultAuthXmlPath(":/authorization_rules.xml");

    const QLatin1String kNonceValidityPeriod("auth/nonceValidityPeriodSec");
    const std::chrono::seconds kDefaultNonceValidityPeriod = std::chrono::hours(4);

    const QLatin1String kIntermediateResponseValidityPeriod("auth/intermediateResponseValidityPeriodSec");
    const std::chrono::seconds kDefaultIntermediateResponseValidityPeriod = std::chrono::minutes(1);
}


namespace nx {
namespace cdb {
namespace conf {

Notification::Notification()
:
    enabled(kDefaultNotificationEnabled)
{
}

SystemManager::SystemManager()
:
    controlSystemStatusByDb(kDefaultControlSystemStatusByDb)
{
}


Settings::Settings()
:
#ifdef _WIN32
    m_settings(
        QSettings::SystemScope,
        QnAppInfo::organizationName(),
        QnLibCloudDbAppInfo::applicationName()),
#else
    m_settings( lit("/opt/%1/%2/etc/%2.conf" )
                .arg(QnAppInfo::linuxOrganizationName()).arg( kModuleName ),
                QSettings::IniFormat ),
#endif
    m_showHelp( false )
{
    fillSupportedCmdParameters();
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

const Auth& Settings::auth() const
{
    return m_auth;
}

const Notification& Settings::notification() const
{
    return m_notification;
}

const AccountManager& Settings::accountManager() const
{
    return m_accountManager;
}

const SystemManager& Settings::systemManager() const
{
    return m_systemManager;
}

const QString& Settings::cloudBackendUrl() const
{
    return m_cloudBackendUrl;
}

const QString& Settings::changeUser() const
{
    return m_changeUser;
}

std::list<SocketAddress> Settings::endpointsToListen() const
{
    const QStringList& httpAddrToListenStrList = m_settings.value(
        kEndpointsToListen,
        kDefaultEndpointsToListen ).toString().split( ',' );
    std::list<SocketAddress> httpAddrToListenList;
    std::transform(
        httpAddrToListenStrList.begin(),
        httpAddrToListenStrList.end(),
        std::back_inserter( httpAddrToListenList ),
        []( const QString& str ) { return SocketAddress( str ); } );

    return httpAddrToListenList;
}

QString Settings::dataDir() const
{
    const QString& dataDirFromSettings = m_settings.value( kDataDir ).toString();
    if( !dataDirFromSettings.isEmpty() )
        return dataDirFromSettings;

#ifdef Q_OS_LINUX
    QString defVarDirName = QString( "/opt/%1/%2/var" )
            .arg(QnAppInfo::linuxOrganizationName()).arg( kModuleName );
    QString varDirName = m_settings.value( "varDir", defVarDirName ).toString();
    return varDirName;
#else
    const QStringList& dataDirList = QStandardPaths::standardLocations( QStandardPaths::DataLocation );
    return dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
}

void Settings::load( int argc, char **argv )
{
    m_commandLineParser.parse(argc, argv, stderr);
    m_settings.parseArgs(argc, argv);

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
}

void Settings::loadConfiguration()
{
    //log
    m_logging.logLevel = m_settings.value( kLogLevel, kDefaultLogLevel ).toString();
    m_logging.logDir = m_settings.value( kLogDir, dataDir() + lit( "/log/" ) ).toString();

    //DB
    m_dbConnectionOptions.driverName = m_settings.value( kDbDriverName, kDefaultDbDriverName ).toString();
    m_dbConnectionOptions.hostName = m_settings.value( kDbHostName, kDefaultDbHostName ).toString();
    m_dbConnectionOptions.port = m_settings.value( kDbPort, kDefaultDbPort ).toInt();
    m_dbConnectionOptions.dbName = m_settings.value( kDbName, kDefaultDbName ).toString();
    m_dbConnectionOptions.userName = m_settings.value( kDbUserName, kDefaultDbUserName ).toString();
    m_dbConnectionOptions.password = m_settings.value( kDbPassword, kDefaultDbPassword ).toString();
    m_dbConnectionOptions.connectOptions = m_settings.value( kDbConnectOptions, kDefaultDbConnectOptions ).toString();
    m_dbConnectionOptions.maxConnectionCount = m_settings.value( kDbMaxConnections, kDefaultDbMaxConnections ).toUInt();
    if( m_dbConnectionOptions.maxConnectionCount == 0 )
        m_dbConnectionOptions.maxConnectionCount = std::thread::hardware_concurrency();

    m_cloudBackendUrl = m_settings.value( kCloudDbUrl, kDefaultCloudDbUrl ).toString();
    m_changeUser = m_settings.value( kChangeUser ).toString();

    //email
    m_notification.enabled =
        m_settings.value(
            kNotificationEnabled,
            kDefaultNotificationEnabled ? "true" : "false").toString() == "true";

    //accountManager
    m_accountManager.passwordResetCodeExpirationTimeout =
        std::chrono::seconds(
            m_settings.value(
                kPasswordResetCodeExpirationTimeout,
                (qlonglong)kDefaultPasswordResetCodeExpirationTimeout.count()).toInt());

    //system manager
    m_systemManager.reportRemovedSystemPeriod =
        std::chrono::seconds(
            m_settings.value(
                kReportRemovedSystemPeriodSec,
                (qlonglong)kDefaultReportRemovedSystemPeriodSec.count()).toInt());

    m_systemManager.notActivatedSystemLivePeriod =
        std::chrono::seconds(
            m_settings.value(
                kNotActivatedSystemLivePeriodSec,
                (qlonglong)kDefaultNotActivatedSystemLivePeriodSec.count()).toInt());

    m_systemManager.dropExpiredSystemsPeriod =
        std::chrono::seconds(
            m_settings.value(
                kDropExpiredSystemsPeriodSec,
                (qlonglong)kDefaultDropExpiredSystemsPeriodSec.count()).toInt());

    m_systemManager.controlSystemStatusByDb =
        m_settings.value(
            kControlSystemStatusByDb,
            kDefaultControlSystemStatusByDb ? "true" : "false").toString() == "true";

    //auth
    m_auth.rulesXmlPath = m_settings.value(kAuthXmlPath, kDefaultAuthXmlPath).toString();
    m_auth.nonceValidityPeriod = 
        std::chrono::seconds(
            m_settings.value(
                kNonceValidityPeriod,
                (int)kDefaultNonceValidityPeriod.count()).toInt());
    m_auth.intermediateResponseValidityPeriod = 
        std::chrono::seconds(
            m_settings.value(
                kIntermediateResponseValidityPeriod,
                (int)kDefaultIntermediateResponseValidityPeriod.count()).toInt());
}

}   //conf
}   //cdb
}   //nx
