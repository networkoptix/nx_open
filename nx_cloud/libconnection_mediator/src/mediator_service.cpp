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

#include <utils/common/command_line_parser.h>
#include <utils/common/log.h>
#include <utils/common/systemerror.h>
#include <utils/network/socket_global.h>

#include "listening_peer_pool.h"
#include "mediaserver_api.h"
#include "version.h"

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
    static const QLatin1String DEFAULT_LOG_LEVEL( "ERROR" );
    static const QLatin1String DEFAULT_STUN_ADDRESS_TO_LISTEN( ":3345" );
    static const QLatin1String DEFAULT_HTTP_ADDRESS_TO_LISTEN( ":3346" );
    static const QLatin1String DEFAULT_CLOUD_USER( "connection_mediator" );
    static const QLatin1String DEFAULT_CLOUD_PASSWORD( "123456" );

    //reading settings
#ifdef _WIN32
    m_settings.reset( new QSettings(QSettings::SystemScope, QN_ORGANIZATION_NAME, QN_APPLICATION_NAME) );
#else
    const QString configFileName = QString("/opt/%1/%2/etc/%2.conf").arg(VER_LINUX_ORGANIZATION_NAME).arg(VER_PRODUCTNAME_STR);
    m_settings.reset( new QSettings(configFileName, QSettings::IniFormat) );
#endif

    const QString& dataLocation = getDataDirectory();

    //parsing command line arguments
    bool showHelp = false;
    bool runWithoutCloud = false;
    QString logLevel;

    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&showHelp, "--help", NULL, QString(), true);
    commandLineParser.addParameter(&logLevel, "--log-level", NULL, QString(), DEFAULT_LOG_LEVEL);
    commandLineParser.addParameter(&runWithoutCloud, "--run-without-cloud", NULL, QString(), true);
    commandLineParser.parse(m_argc, m_argv, stderr);

    if( showHelp )
        return printHelp();

    //logging
    if( logLevel != QString::fromLatin1("none") )
    {
        const QString& logDir = m_settings->value( "logDir", dataLocation + QLatin1String("/log/") ).toString();
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
        cloudDataProvider = std::make_unique< CloudDataProvider >(
            m_settings->value("cloudUser", DEFAULT_CLOUD_USER).toString().toStdString(),
            m_settings->value("cloudPassword", DEFAULT_CLOUD_PASSWORD).toString().toStdString() );
    else
        NX_LOG( lit( "STUN Server is running without cloud (debug mode)" ), cl_logALWAYS );

    //STUN handlers
    stun::MessageDispatcher stunMessageDispatcher;
    MediaserverApi mediaserverApi( cloudDataProvider.get(), &stunMessageDispatcher );
    ListeningPeerPool listeningPeerPool( cloudDataProvider.get(), &stunMessageDispatcher );

    //accepting STUN requests by both tcp and udt
    m_multiAddressStunServer.reset(
        new MultiAddressServer<stun::SocketServer>( false, SocketFactory::nttDisabled ) );

    if( !m_multiAddressStunServer->bind( stunAddrToListenList ) )
        return 2;

    //TODO: #ak process privilege reduction should be made here

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
    QString defVarDirName = QString("/opt/%1/%2/var").arg(VER_LINUX_ORGANIZATION_NAME).arg(VER_PRODUCTNAME_STR);
    QString varDirName = m_settings->value("varDir", defVarDirName).toString();
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
