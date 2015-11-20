/**********************************************************
* 27 aug 2013
* a.kolesnikov
***********************************************************/

#include "mediator_service.h"

#include <algorithm>
#include <iostream>
#include <list>

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <platform/process/current_process.h>
#include <utils/common/command_line_parser.h>
#include <utils/common/log.h>
#include <utils/common/systemerror.h>
#include <nx/network/socket_global.h>

#include "listening_peer_pool.h"
#include "mediaserver_api.h"
#include "version.h"

static const QLatin1String CLOUD_ADDRESS( "cloudAddress" );
static const QLatin1String CLOUD_USER( "cloudUser" );
static const QLatin1String CLOUD_PASSWORD( "cloudPassword" );
static const QLatin1String CLOUD_UPDATE_INTERVAL( "cloudUpdateInterval" );

static const QLatin1String DEFAULT_LOG_LEVEL( "ERROR" );
static const QLatin1String DEFAULT_STUN_ADDRESS_TO_LISTEN( ":3345" );
static const QLatin1String DEFAULT_HTTP_ADDRESS_TO_LISTEN( ":3346" );
static const QLatin1String DEFAULT_CLOUD_USER( "connection_mediator" );
static const QLatin1String DEFAULT_CLOUD_PASSWORD( "123456" );

#ifdef Q_OS_LINUX
static const QString MODULE_NAME = lit( "connection_mediator" );
static const QString DEFAULT_CONFIG_FILE = QString( "/opt/%1/%2/etc/%2.conf" )
        .arg( VER_LINUX_ORGANIZATION_NAME ).arg( MODULE_NAME );
static const QString DEFAULT_VAR_DIR = QString( "/opt/%1/%2/var" )
        .arg( VER_LINUX_ORGANIZATION_NAME ).arg( MODULE_NAME );
#endif

namespace nx {
namespace hpm {

MediatorProcess::MediatorProcess( int argc, char **argv )
:
    QtService<QtSingleCoreApplication>(argc, argv, QN_APPLICATION_NAME),
    m_argc( argc ),
    m_argv( argv )
{
    setServiceDescription(QN_APPLICATION_NAME);
}

void MediatorProcess::pleaseStop()
{
    application()->quit();
}

int MediatorProcess::executeApplication()
{
    bool showHelp = false;
    bool runWithoutCloud = false;
    QString logLevel;
    QString configFile;

    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&showHelp, "--help", NULL, QString(), true);
    commandLineParser.addParameter(&logLevel, "--log-level", NULL, QString(), DEFAULT_LOG_LEVEL);
    commandLineParser.addParameter(&configFile, "--configFile", NULL, QString(), QString());
    commandLineParser.addParameter(&runWithoutCloud, "--run-without-cloud", NULL, QString(), true);
    commandLineParser.parse(m_argc, m_argv, stderr);

    if( showHelp )
        return printHelp();

    if( configFile.isEmpty() )
    {
        #ifdef _WIN32
            m_settings.reset( new QSettings( QSettings::SystemScope,
                                             QN_ORGANIZATION_NAME, QN_APPLICATION_NAME ) );
        #else
            m_settings.reset( new QSettings( DEFAULT_CONFIG_FILE,
                                             QSettings::IniFormat ) );
        #endif
    }
    else
    {
        m_settings.reset( new QSettings( configFile, QSettings::IniFormat ) );
    }

    if( logLevel != QString::fromLatin1("none") )
    {
        const auto defaultLogDir = getDataDirectory() + QLatin1String("/log/");
        const auto logDir = m_settings->value( "logDir", defaultLogDir ).toString();
        QDir().mkpath( logDir );
        const QString& logFileName = logDir + QLatin1String("/log_file");
        if( cl_log.create( logFileName, 1024 * 1024 * 10, 5, cl_logDEBUG1 ) )
            QnLog::initLog( logLevel );
        else
            std::wcerr<<L"Failed to create log file "<<logFileName.toStdWString()<<std::endl;
        NX_LOG( lit( "=================================================================================" ), cl_logALWAYS );
        NX_LOG( lit( "%1 started" ).arg( QN_APPLICATION_NAME ), cl_logALWAYS );
        NX_LOG( lit( "Software version: %1" ).arg( QN_APPLICATION_VERSION ), cl_logALWAYS );
        NX_LOG( lit( "Software revision: %1" ).arg( QN_APPLICATION_REVISION ), cl_logALWAYS );
    }

    const QStringList& stunAddrToListenStrList = m_settings->value("stunListenOn", DEFAULT_STUN_ADDRESS_TO_LISTEN).toString().split(',');
    const QStringList& httpAddrToListenStrList = m_settings->value("httpListenOn", DEFAULT_HTTP_ADDRESS_TO_LISTEN).toString().split(',');
    std::list<SocketAddress> stunAddrToListenList;
    std::transform(
        stunAddrToListenStrList.begin(),
        stunAddrToListenStrList.end(),
        std::back_inserter(stunAddrToListenList),
        [] (const QString& str) { return SocketAddress(str); } );
    if( stunAddrToListenList.empty() )
    {
        NX_LOG( "No STUN address to listen", cl_logALWAYS );
        return 1;
    }

    std::list<SocketAddress> httpAddrToListenList;
    std::transform(
        httpAddrToListenStrList.begin(),
        httpAddrToListenStrList.end(),
        std::back_inserter(httpAddrToListenList),
        [] (const QString& str) { return SocketAddress(str); } );
    if( httpAddrToListenList.empty() )
    {
        NX_LOG( "No HTTP address to listen", cl_logALWAYS );
        return 1;
    }

    TimerManager timerManager;
    std::shared_ptr< CloudDataProvider > cloudDataProvider;
    if( !runWithoutCloud )
    {
        auto address = m_settings->value( CLOUD_ADDRESS, lit("") ).toString().toStdString();
        auto user = m_settings->value( CLOUD_USER, DEFAULT_CLOUD_USER ).toString().toStdString();
        auto password = m_settings->value( CLOUD_PASSWORD, DEFAULT_CLOUD_PASSWORD ).toString().toStdString();
        auto update = parseTimerDuration(
            m_settings->value( CLOUD_UPDATE_INTERVAL ).toString(),
            CloudDataProvider::DEFAULT_UPDATE_INTERVAL );

        cloudDataProvider = std::make_unique< CloudDataProvider >(
            std::move(address), std::move(user), std::move(password), std::move(update) );
    }
    else
        NX_LOG( lit( "STUN Server is running without cloud (debug mode)" ), cl_logALWAYS );

    //STUN handlers
    stun::MessageDispatcher stunMessageDispatcher;
    MediaserverApi mediaserverApi( cloudDataProvider.get(), &stunMessageDispatcher );
    ListeningPeerPool listeningPeerPool( cloudDataProvider.get(), &stunMessageDispatcher );

    //accepting STUN requests by both tcp and udt
    m_multiAddressStunServer.reset(
        new MultiAddressServer<stun::SocketServer>( false, SocketFactory::NatTraversalType::nttDisabled ) );

    if( !m_multiAddressStunServer->bind( stunAddrToListenList ) )
        return 2;

    // process privilege reduction
    CurrentProcess::changeUser( m_settings->value( lit("changeUser") ).toString() );

    if( !m_multiAddressStunServer->listen() )
        return 4;

    NX_LOG( lit( "STUN Server is listening %1" )
            .arg( containerString( stunAddrToListenList ) ), cl_logALWAYS );

    //TODO #ak remove qt event loop
    //application's main loop
    const int result = application()->exec();

    //stopping accepting incoming connections
    m_multiAddressStunServer.reset();

    return result;
}

void MediatorProcess::start()
{
    QtSingleCoreApplication* application = this->application();

    if( application->isRunning() )
    {
        NX_LOG( "Server already started", cl_logERROR );
        application->quit();
        return;
    }
}

void MediatorProcess::stop()
{
    application()->quit();
}

QString MediatorProcess::getDataDirectory()
{
    const QString& dataDirFromSettings = m_settings->value( "dataDir" ).toString();
    if( !dataDirFromSettings.isEmpty() )
        return dataDirFromSettings;

#ifdef Q_OS_LINUX
    QString varDirName = m_settings->value("varDir", DEFAULT_VAR_DIR).toString();
    return varDirName;
#else
    const QStringList& dataDirList = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
    return dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
}

int MediatorProcess::printHelp()
{
    //TODO #ak

    return 0;
}

} // namespace hpm
} // namespace nx
