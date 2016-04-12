/**********************************************************
* Dec 21, 2015
* a.kolesnikov
***********************************************************/

#include "settings.h"

#include <thread>

#include <QtCore/QLatin1String>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>

#include <nx/utils/timer_manager.h>
#include <utils/serialization/lexical.h>

#include <libconnection_mediator_app_info.h>
#include <utils/common/app_info.h>


namespace
{
    //General settings
    const QLatin1String kConfigFilePath("general/configFile");
    const QLatin1String kDefaultConfigFilePath("");

    const QLatin1String kSystemUserToRunUnder("general/systemUserToRunUnder");
    const QLatin1String kDefaultSystemUserToRunUnder("");

    const QLatin1String kDataDir("dataDir");
    const QLatin1String kDefaultDataDir("");

    //log settings
    const QLatin1String kLogLevel("log/logLevel");
#ifdef _DEBUG
    const QLatin1String kDefaultLogLevel("DEBUG");
#else
    const QLatin1String kDefaultLogLevel("INFO");
#endif
    const QLatin1String kLogDir("log/logDir");
    const QLatin1String kDefaultLogDir("");

    //CloudDB settings
    const QLatin1String kRunWithCloud("cloud_db/runWithCloud");
    const QLatin1String kDefaultRunWithCloud("true");

    const QLatin1String kCdbAddress("cloud_db/address");
    const QLatin1String kDefaultCdbAddress("");

    const QLatin1String kCdbUser("cloud_db/user");
    const QLatin1String kDefaultCdbUser("connection_mediator");

    const QLatin1String kCdbPassword("cloud_db/password");
    const QLatin1String kDefaultCdbPassword("123456");

    const QLatin1String kCdbUpdateInterval("cloud_db/updateIntervalSec");
    const std::chrono::seconds kDefaultCdbUpdateInterval(std::chrono::minutes(10));

    //STUN
    const QLatin1String kStunEndpointsToListen("stun/addrToListenList");
    const QLatin1String kDefaultStunEndpointsToListen("0.0.0.0:3345");

    //HTTP
    const QLatin1String kHttpEndpointsToListen("http/addrToListenList");
    const QLatin1String kDefaultHttpEndpointsToListen("0.0.0.0:3355");

    const QString kModuleName = lit("connection_mediator");
}


namespace nx {
namespace hpm {
namespace conf {

Settings::Settings()
:
#ifdef _WIN32
    m_settings(
        QSettings::SystemScope,
        QnAppInfo::organizationName(),
        QnLibConnectionMediatorAppInfo::applicationName()),
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

const General& Settings::general() const
{
    return m_general;
}

const CloudDB& Settings::cloudDB() const
{
    return m_cloudDB;
}

const Stun& Settings::stun() const
{
    return m_stun;
}

const Http& Settings::http() const
{
    return m_http;
}

const Logging& Settings::logging() const
{
    return m_logging;
}

void Settings::load(int argc, char **argv)
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
    using namespace std::chrono;

    m_general.configFilePath = m_settings.value(
        kConfigFilePath,
        kDefaultConfigFilePath).toString();
    m_general.systemUserToRunUnder = m_settings.value(
        kSystemUserToRunUnder,
        kDefaultSystemUserToRunUnder).toString();
    m_general.dataDir = m_settings.value(
        kDataDir,
        kDefaultDataDir).toString();

    //log
    m_logging.logLevel = m_settings.value(kLogLevel, kDefaultLogLevel).toString();
    m_logging.logDir = m_settings.value(kLogDir, kDefaultLogDir).toString();

    m_cloudDB.runWithCloud = m_settings.value(kRunWithCloud, kDefaultRunWithCloud).toBool();
    m_cloudDB.address = m_settings.value(kCdbAddress, kDefaultCdbAddress).toString();
    m_cloudDB.user = m_settings.value(kCdbUser, kDefaultCdbUser).toString();
    m_cloudDB.password = m_settings.value(kCdbPassword, kDefaultCdbPassword).toString();
    m_cloudDB.updateInterval = duration_cast<seconds>(
        parseTimerDuration(m_settings.value(
            kCdbUpdateInterval,
            static_cast<qulonglong>(kDefaultCdbUpdateInterval.count())).toString()));

    readEndpointList(
        m_settings.value(kStunEndpointsToListen, kDefaultStunEndpointsToListen).toString(),
        &m_stun.addrToListenList);

    readEndpointList(
        m_settings.value(kHttpEndpointsToListen, kDefaultHttpEndpointsToListen).toString(),
        &m_http.addrToListenList);

    //analyzing values
    if (m_general.dataDir.isEmpty())
    {
#ifdef Q_OS_LINUX
        m_general.dataDir = QString("/opt/%1/%2/var")
            .arg(QnAppInfo::linuxOrganizationName()).arg(kModuleName);
#else
        const QStringList& dataDirList = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
        m_general.dataDir = dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
    }

    if (m_logging.logDir.isEmpty())
        m_logging.logDir = m_general.dataDir + lit("/log/");
}

void Settings::readEndpointList(
    const QString& str,
    std::list<SocketAddress>* const addrToListenList)
{
    const QStringList& httpAddrToListenStrList = str.split(',');
    std::transform(
        httpAddrToListenStrList.begin(),
        httpAddrToListenStrList.end(),
        std::back_inserter(*addrToListenList),
        [](const QString& str) { return SocketAddress(str); });
}

}   //conf
}   //hpm
}   //nx
