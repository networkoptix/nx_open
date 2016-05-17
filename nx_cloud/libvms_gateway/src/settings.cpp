/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#include "settings.h"

#include <thread>

#include <QtCore/QLatin1String>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>

#include <nx/utils/timer_manager.h>
#include <libvms_gateway_app_info.h>
#include <utils/serialization/lexical.h>
#include <utils/common/app_info.h>


namespace
{
    //general
    const QLatin1String kEndpointsToListen("listenOn");
    const QLatin1String kDefaultEndpointsToListen("0.0.0.0:3347");

    const QLatin1String kDataDir("dataDir");

    const QLatin1String kChangeUser("changeUser");

    //log settings
    const QLatin1String kLogLevel( "log/logLevel" );
#ifdef _DEBUG
    const QLatin1String kDefaultLogLevel( "DEBUG" );
#else
    const QLatin1String kDefaultLogLevel( "INFO" );
#endif
    const QLatin1String kLogDir( "log/logDir" );

    const QLatin1String kAuthXmlPath("auth/rulesXmlPath");
    const QLatin1String kDefaultAuthXmlPath(":/authorization_rules.xml");
}


namespace nx {
namespace cloud {
namespace gateway {
namespace conf {

Settings::Settings()
:
#ifdef _WIN32
    m_settings(
        QSettings::SystemScope,
        QnAppInfo::organizationName(),
        QnLibVmsGatewayAppInfo::applicationName()),
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

const Logging& Settings::logging() const
{
    return m_logging;
}

const Auth& Settings::auth() const
{
    return m_auth;
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
    using namespace std::chrono;

    //general
    const QStringList& httpAddrToListenStrList = m_settings.value(
        kEndpointsToListen,
        kDefaultEndpointsToListen).toString().split(',');
    std::transform(
        httpAddrToListenStrList.begin(),
        httpAddrToListenStrList.end(),
        std::back_inserter(m_general.endpointsToListen),
        [](const QString& str) { return SocketAddress(str); });

    const QString& dataDirFromSettings = m_settings.value(kDataDir).toString();
    if (!dataDirFromSettings.isEmpty())
    {
        m_general.dataDir = dataDirFromSettings;
    }
    else
    {
#ifdef Q_OS_LINUX
        QString defVarDirName = QString("/opt/%1/%2/var")
            .arg(QnAppInfo::linuxOrganizationName()).arg(kModuleName);
        QString varDirName = m_settings.value("varDir", defVarDirName).toString();
        m_general.dataDir = varDirName;
#else
        const QStringList& dataDirList = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
        m_general.dataDir = dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
    }

    m_general.changeUser = m_settings.value(kChangeUser).toString();

    //log
    m_logging.logLevel = m_settings.value(kLogLevel, kDefaultLogLevel).toString();
    m_logging.logDir = m_settings.value(
        kLogDir,
        m_general.dataDir + lit("/log/")).toString();

    //auth
    m_auth.rulesXmlPath = m_settings.value(kAuthXmlPath, kDefaultAuthXmlPath).toString();
}

}   //namespace conf
}   //namespace cloud
}   //namespace gateway
}   //namespace nx
