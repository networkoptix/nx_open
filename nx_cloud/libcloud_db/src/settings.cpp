/**********************************************************
* Jul 31, 2015
* a.kolesnikov
***********************************************************/

#include "settings.h"

#include <thread>

#include <QtCore/QLatin1String>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>

#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/timer_manager.h>
#include <libcloud_db_app_info.h>
#include <utils/common/app_info.h>


namespace {

const QLatin1String kEndpointsToListen("listenOn");
const QLatin1String kDefaultEndpointsToListen("0.0.0.0:3346");

const QLatin1String kDataDir("dataDir");

//DB options
const QLatin1String kDbDriverName("db/driverName");
const QLatin1String kDefaultDbDriverName("QMYSQL");

const QLatin1String kDbHostName("db/hostName");
const QLatin1String kDefaultDbHostName("127.0.0.1");

const QLatin1String kDbPort("db/port");
const int kDefaultDbPort = 3306;

const QLatin1String kDbName("db/name");
const QLatin1String kDefaultDbName("nx_cloud");

const QLatin1String kDbUserName("db/userName");
const QLatin1String kDefaultDbUserName("root");

const QLatin1String kDbPassword("db/password");
const QLatin1String kDefaultDbPassword("");

const QLatin1String kDbConnectOptions("db/connectOptions");
const QLatin1String kDefaultDbConnectOptions("");

const QLatin1String kDbMaxConnections("db/maxConnections");
const QLatin1String kDefaultDbMaxConnections("1");

const QLatin1String kDbInactivityTimeout("db/inactivityTimeout");
const std::chrono::seconds kDefaultDbInactivityTimeout = std::chrono::minutes(10);

const QLatin1String kDbMaxPeriodQueryWaitsForAvailableConnection(
    "db/maxPeriodQueryWaitsForAvailableConnection");
const std::chrono::seconds kDefaultDbMaxPeriodQueryWaitsForAvailableConnection =
std::chrono::minutes(1);

const QLatin1String kChangeUser("changeUser");

//notification settings
const QLatin1String kNotificationServiceEndpoint("notification/serviceEndpoint");
const QLatin1String kNotificationSecret("notification/secret");

const QLatin1String kNotificationEnabled("notification/enabled");
const bool kDefaultNotificationEnabled = true;

//account manager
const QLatin1String kPasswordResetCodeExpirationTimeout("accountManager/passwordResetCodeExpirationTimeout");
const std::chrono::seconds kDefaultPasswordResetCodeExpirationTimeout = std::chrono::hours(24);

const QLatin1String kReportRemovedSystemPeriodSec("systemManager/reportRemovedSystemPeriod");
const std::chrono::seconds kDefaultReportRemovedSystemPeriodSec = std::chrono::hours(30 * 24);  //a month

const QLatin1String kNotActivatedSystemLivePeriodSec("systemManager/notActivatedSystemLivePeriod");
const std::chrono::seconds kDefaultNotActivatedSystemLivePeriodSec = std::chrono::hours(30 * 24);  //a month

const QLatin1String kDropExpiredSystemsPeriodSec("systemManager/dropExpiredSystemsPeriod");
const std::chrono::seconds kDefaultDropExpiredSystemsPeriodSec = std::chrono::hours(12);

const QLatin1String kControlSystemStatusByDb("systemManager/controlSystemStatusByDb");
const bool kDefaultControlSystemStatusByDb = false;

//auth settings
const QString kModuleName = lit("cloud_db");

const QLatin1String kAuthXmlPath("auth/rulesXmlPath");
const QLatin1String kDefaultAuthXmlPath(":/authorization_rules.xml");

const QLatin1String kNonceValidityPeriod("auth/nonceValidityPeriod");
const std::chrono::seconds kDefaultNonceValidityPeriod = std::chrono::hours(4);

const QLatin1String kIntermediateResponseValidityPeriod("auth/intermediateResponseValidityPeriod");
const std::chrono::seconds kDefaultIntermediateResponseValidityPeriod = std::chrono::minutes(1);

const QLatin1String kConnectionInactivityPeriod("auth/connectionInactivityPeriod");
const std::chrono::milliseconds kDefaultConnectionInactivityPeriod(0); //< disabled


//event manager settings
const QLatin1String kMediaServerConnectionIdlePeriod("eventManager/mediaServerConnectionIdlePeriod");
const auto kDefaultMediaServerConnectionIdlePeriod = std::chrono::minutes(1);

} // namespace


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


EventManager::EventManager()
:
    mediaServerConnectionIdlePeriod(kDefaultMediaServerConnectionIdlePeriod)
{
}


Settings::Settings()
:
    m_settings(QnLibCloudDbAppInfo::applicationName(), kModuleName),
    m_showHelp(false)
{
    fillSupportedCmdParameters();
}

bool Settings::showHelp() const
{
    return m_showHelp;
}

const QnLogSettings& Settings::logging() const
{
    return m_logging;
}

const QnLogSettings& Settings::vmsSynchronizationLogging() const
{
    return m_vmsSynchronizationLogging;
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

const EventManager& Settings::eventManager() const
{
    return m_eventManager;
}

const ec2::Settings& Settings::p2pDb() const
{
    return m_p2pDb;
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
    m_settings.parseArgs(argc, (const char**)argv);

    loadConfiguration();
}

void Settings::printCmdLineArgsHelpToCout()
{
    // TODO: #ak
}

void Settings::fillSupportedCmdParameters()
{
    m_commandLineParser.addParameter(
        &m_showHelp, "--help", NULL, "Show help message", false );
}

void Settings::loadConfiguration()
{
    using namespace std::chrono;

    //log
    m_logging.load(m_settings, QLatin1String("log"));
    m_vmsSynchronizationLogging.load(m_settings, QLatin1String("syncroLog"));

    //DB
    m_dbConnectionOptions.driverType =
        QnLexical::deserialized<nx::db::RdbmsDriverType>(
            m_settings.value(kDbDriverName, kDefaultDbDriverName).toString(),
            nx::db::RdbmsDriverType::unknown);
    //< Ignoring error here since connection to DB will not be established anyway.
    m_dbConnectionOptions.hostName = m_settings.value(kDbHostName, kDefaultDbHostName).toString();
    m_dbConnectionOptions.port = m_settings.value(kDbPort, kDefaultDbPort).toInt();
    m_dbConnectionOptions.dbName = m_settings.value(kDbName, kDefaultDbName).toString();
    m_dbConnectionOptions.userName = m_settings.value(kDbUserName, kDefaultDbUserName).toString();
    m_dbConnectionOptions.password = m_settings.value(kDbPassword, kDefaultDbPassword).toString();
    m_dbConnectionOptions.connectOptions = m_settings.value(kDbConnectOptions, kDefaultDbConnectOptions).toString();
    m_dbConnectionOptions.maxConnectionCount = m_settings.value(kDbMaxConnections, kDefaultDbMaxConnections).toUInt();
    if (m_dbConnectionOptions.maxConnectionCount == 0)
        m_dbConnectionOptions.maxConnectionCount = std::thread::hardware_concurrency();
    m_dbConnectionOptions.inactivityTimeout = duration_cast<seconds>(
        nx::utils::parseTimerDuration(
            m_settings.value(kDbInactivityTimeout).toString(),
            kDefaultDbInactivityTimeout));
    m_dbConnectionOptions.maxPeriodQueryWaitsForAvailableConnection = duration_cast<seconds>(
        nx::utils::parseTimerDuration(
            m_settings.value(kDbMaxPeriodQueryWaitsForAvailableConnection).toString(),
            kDefaultDbMaxPeriodQueryWaitsForAvailableConnection));
    

    m_changeUser = m_settings.value(kChangeUser).toString();

    //email
    m_notification.serviceEndpoint =
        m_settings.value(kNotificationServiceEndpoint).toString();

    m_notification.secret =
        m_settings.value(kNotificationSecret).toString();

    m_notification.enabled =
        m_settings.value(
            kNotificationEnabled,
            kDefaultNotificationEnabled ? "true" : "false").toString() == "true";

    //accountManager
    m_accountManager.passwordResetCodeExpirationTimeout = duration_cast<seconds>(
        nx::utils::parseTimerDuration(
            m_settings.value(kPasswordResetCodeExpirationTimeout).toString(),
            kDefaultPasswordResetCodeExpirationTimeout));

    //system manager
    m_systemManager.reportRemovedSystemPeriod = duration_cast<seconds>(
        nx::utils::parseTimerDuration(
            m_settings.value(kReportRemovedSystemPeriodSec).toString(),
            kDefaultReportRemovedSystemPeriodSec));

    m_systemManager.notActivatedSystemLivePeriod = duration_cast<seconds>(
        nx::utils::parseTimerDuration(
            m_settings.value(kNotActivatedSystemLivePeriodSec).toString(),
            kDefaultNotActivatedSystemLivePeriodSec));

    m_systemManager.dropExpiredSystemsPeriod = duration_cast<seconds>(
        nx::utils::parseTimerDuration(
            m_settings.value(kDropExpiredSystemsPeriodSec).toString(),
            kDefaultDropExpiredSystemsPeriodSec));

    m_systemManager.controlSystemStatusByDb =
        m_settings.value(
            kControlSystemStatusByDb,
            kDefaultControlSystemStatusByDb ? "true" : "false").toString() == "true";

    //auth
    m_auth.rulesXmlPath = m_settings.value(kAuthXmlPath, kDefaultAuthXmlPath).toString();
    m_auth.nonceValidityPeriod = duration_cast<seconds>(
        nx::utils::parseTimerDuration(
            m_settings.value(kNonceValidityPeriod).toString(),
            kDefaultNonceValidityPeriod));
    m_auth.intermediateResponseValidityPeriod = duration_cast<seconds>(
        nx::utils::parseTimerDuration(
            m_settings.value(kIntermediateResponseValidityPeriod).toString(),
            kDefaultIntermediateResponseValidityPeriod));
    m_auth.connectionInactivityPeriod = duration_cast<seconds>(
        nx::utils::parseTimerDuration(
            m_settings.value(kConnectionInactivityPeriod).toString(),
            kDefaultConnectionInactivityPeriod));

    //event manager
    m_eventManager.mediaServerConnectionIdlePeriod = duration_cast<seconds>(
        nx::utils::parseTimerDuration(
            m_settings.value(kMediaServerConnectionIdlePeriod).toString(),
            kDefaultMediaServerConnectionIdlePeriod));

    m_p2pDb.load(m_settings);
}

} // namespace conf
} // namespace cdb
} // namespace nx
