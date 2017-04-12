#include "settings.h"

#include <thread>

#include <QtCore/QLatin1String>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>

#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/timer_manager.h>

#include <utils/common/app_info.h>

#include "libcloud_db_app_info.h"

namespace {

const QLatin1String kEndpointsToListen("listenOn");
const QLatin1String kDefaultEndpointsToListen("0.0.0.0:3346");

const QLatin1String kDataDir("dataDir");

const QLatin1String kChangeUser("changeUser");

//-------------------------------------------------------------------------------------------------
// Notifications settings
const QLatin1String kNotificationUrl("notification/url");

const QLatin1String kNotificationEnabled("notification/enabled");
const bool kDefaultNotificationEnabled = true;

//-------------------------------------------------------------------------------------------------
// Account manager settings
const QLatin1String kAccountActivationCodeExpirationTimeout("accountManager/accountActivationCodeExpirationTimeout");
const std::chrono::seconds kDefaultAccountActivationCodeExpirationTimeout = std::chrono::hours(3*24);

const QLatin1String kPasswordResetCodeExpirationTimeout("accountManager/passwordResetCodeExpirationTimeout");
const std::chrono::seconds kDefaultPasswordResetCodeExpirationTimeout = std::chrono::hours(24);

//-------------------------------------------------------------------------------------------------
// System manager settings
const QLatin1String kReportRemovedSystemPeriodSec("systemManager/reportRemovedSystemPeriod");
const std::chrono::seconds kDefaultReportRemovedSystemPeriodSec = std::chrono::hours(30 * 24);  //a month

const QLatin1String kNotActivatedSystemLivePeriodSec("systemManager/notActivatedSystemLivePeriod");
const std::chrono::seconds kDefaultNotActivatedSystemLivePeriodSec = std::chrono::hours(30 * 24);  //a month

const QLatin1String kDropExpiredSystemsPeriodSec("systemManager/dropExpiredSystemsPeriod");
const std::chrono::seconds kDefaultDropExpiredSystemsPeriodSec = std::chrono::hours(12);

const QLatin1String kControlSystemStatusByDb("systemManager/controlSystemStatusByDb");
const bool kDefaultControlSystemStatusByDb = false;

//-------------------------------------------------------------------------------------------------
// Authentication settings
const QString kModuleName = lit("cloud_db");

const QLatin1String kAuthXmlPath("auth/rulesXmlPath");
const QLatin1String kDefaultAuthXmlPath(":/authorization_rules.xml");

const QLatin1String kNonceValidityPeriod("auth/nonceValidityPeriod");
const std::chrono::seconds kDefaultNonceValidityPeriod = std::chrono::hours(4);

const QLatin1String kIntermediateResponseValidityPeriod("auth/intermediateResponseValidityPeriod");
const std::chrono::seconds kDefaultIntermediateResponseValidityPeriod = std::chrono::minutes(1);

const QLatin1String kConnectionInactivityPeriod("auth/connectionInactivityPeriod");
const std::chrono::milliseconds kDefaultConnectionInactivityPeriod(0); //< disabled

//-------------------------------------------------------------------------------------------------
// Event manager settings
const QLatin1String kMediaServerConnectionIdlePeriod("eventManager/mediaServerConnectionIdlePeriod");
const auto kDefaultMediaServerConnectionIdlePeriod = std::chrono::minutes(1);

//-------------------------------------------------------------------------------------------------
// Module finder
const QLatin1String kCloudModuleXmlTemplatePath("moduleFinder/cloudModuleXmlTemplatePath");
const QLatin1String kDefaultCloudModuleXmlTemplatePath(":/cloud_modules_template.xml");

//-------------------------------------------------------------------------------------------------
// Http
const QLatin1String kTcpBacklogSize("http/tcpBacklogSize");
const int kDefaultTcpBacklogSize = 1024;

} // namespace


namespace nx {
namespace cdb {
namespace conf {

Notification::Notification():
    enabled(kDefaultNotificationEnabled)
{
}

AccountManager::AccountManager():
    accountActivationCodeExpirationTimeout(kDefaultAccountActivationCodeExpirationTimeout),
    passwordResetCodeExpirationTimeout(kDefaultPasswordResetCodeExpirationTimeout)
{
}

SystemManager::SystemManager():
    controlSystemStatusByDb(kDefaultControlSystemStatusByDb)
{
}

EventManager::EventManager():
    mediaServerConnectionIdlePeriod(kDefaultMediaServerConnectionIdlePeriod)
{
}

Http::Http():
    tcpBacklogSize(kDefaultTcpBacklogSize)
{
}

Settings::Settings():
    m_settings(
        QnAppInfo::organizationNameForSettings(),
        QnLibCloudDbAppInfo::applicationName(),
        kModuleName),
    m_showHelp(false)
{
    fillSupportedCmdParameters();

    m_dbConnectionOptions.driverType = nx::db::RdbmsDriverType::mysql;
    m_dbConnectionOptions.hostName = "127.0.0.1";
    m_dbConnectionOptions.port = 3306;
    m_dbConnectionOptions.dbName = "nx_cloud";
    m_dbConnectionOptions.userName = "root";
    m_dbConnectionOptions.encoding = "utf8";
    m_dbConnectionOptions.maxConnectionCount = 1;
    m_dbConnectionOptions.inactivityTimeout = std::chrono::minutes(10);
    m_dbConnectionOptions.maxPeriodQueryWaitsForAvailableConnection = std::chrono::minutes(1);
}

void Settings::load(int argc, char **argv)
{
    m_commandLineParser.parse(argc, const_cast<const char**>(argv), stderr);
    m_settings.parseArgs(argc, const_cast<const char**>(argv));

    loadConfiguration();
}

bool Settings::isShowHelpRequested() const
{
    return m_showHelp;
}

void Settings::printCmdLineArgsHelp()
{
    // TODO: #ak
}

QString Settings::dataDir() const
{
    const QString& dataDirFromSettings = m_settings.value(kDataDir).toString();
    if (!dataDirFromSettings.isEmpty())
        return dataDirFromSettings;

#ifdef Q_OS_LINUX
    QString defVarDirName = QString("/opt/%1/%2/var")
        .arg(QnAppInfo::linuxOrganizationName()).arg(kModuleName);
    QString varDirName = m_settings.value("varDir", defVarDirName).toString();
    return varDirName;
#else
    const QStringList& dataDirList = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
    return dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
}

nx::utils::log::Settings Settings::logging() const
{
    return m_logging;
}

const nx::utils::log::Settings& Settings::vmsSynchronizationLogging() const
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

const ModuleFinder& Settings::moduleFinder() const
{
    return m_moduleFinder;
}

const Http& Settings::http() const
{
    return m_http;
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
    m_dbConnectionOptions.loadFromSettings(&m_settings);

    m_changeUser = m_settings.value(kChangeUser).toString();

    //email
    m_notification.url = m_settings.value(kNotificationUrl).toString();
    m_notification.enabled =
        m_settings.value(
            kNotificationEnabled,
            kDefaultNotificationEnabled ? "true" : "false").toString() == "true";

    //accountManager
    m_accountManager.accountActivationCodeExpirationTimeout = duration_cast<seconds>(
        nx::utils::parseTimerDuration(
            m_settings.value(kAccountActivationCodeExpirationTimeout).toString(),
            kDefaultAccountActivationCodeExpirationTimeout));

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

    m_moduleFinder.cloudModulesXmlTemplatePath = m_settings.value(
        kCloudModuleXmlTemplatePath, kDefaultCloudModuleXmlTemplatePath).toString();

    m_http.tcpBacklogSize = m_settings.value(
        kTcpBacklogSize, kDefaultTcpBacklogSize).toInt();
}

} // namespace conf
} // namespace cdb
} // namespace nx
