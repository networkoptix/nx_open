/**********************************************************
* 8 may 2015
* a.kolesnikov
***********************************************************/

#include "cloud_db_process.h"

#include <algorithm>
#include <iostream>
#include <list>

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <utils/common/command_line_parser.h>
#include <utils/common/log.h>
#include <utils/common/systemerror.h>
#include <utils/network/aio/aioservice.h>
#include <utils/network/http/server/http_message_dispatcher.h>
#include <utils/network/http/server/server_managers.h>

#include "access_control/authentication_manager.h"
#include "http_handlers/add_account_handler.h"

#include "version.h"


CloudDBProcess::CloudDBProcess( int argc, char **argv )
: 
    QtService<QtSingleCoreApplication>(argc, argv, QN_APPLICATION_NAME),
    m_argc( argc ),
    m_argv( argv )
{
    setServiceDescription(QN_APPLICATION_NAME);
}

void CloudDBProcess::pleaseStop()
{
    application()->quit();
}

int CloudDBProcess::executeApplication()
{
    static const QLatin1String DEFAULT_LOG_LEVEL( "ERROR" );
    static const QLatin1String DEFAULT_HTTP_ADDRESS_TO_LISTEN( ":3346" );

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
    QString logLevel;

    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&showHelp, "--help", NULL, QString(), false);
    commandLineParser.addParameter(&logLevel, "--log-level", NULL, QString(), DEFAULT_LOG_LEVEL);
    commandLineParser.parse(m_argc, m_argv, stderr);

    if( showHelp )
        return printHelp();

    initializeLogging( dataLocation, logLevel );

    const QStringList& httpAddrToListenStrList = m_settings->value("httpListenOn", DEFAULT_HTTP_ADDRESS_TO_LISTEN).toString().split(',');
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

    nx_http::ServerManagers httpServerManagers;

    nx_http::MessageDispatcher httpMessageDispatcher;
    httpServerManagers.setDispatcher( &httpMessageDispatcher );

    AuthenticationManager authenticationManager;
    httpServerManagers.setAuthenticationManager( &authenticationManager );

    //TODO #ak registering HTTP handlers
    cdb_api::AddAccountHttpHandler addAccountHandler;
    httpMessageDispatcher.registerRequestProcessor(
        cdb_api::AddAccountHttpHandler::HANDLER_PATH,
        &addAccountHandler );

    using namespace std::placeholders;

    m_multiAddressHttpServer.reset( new MultiAddressServer<nx_http::HttpStreamSocketServer>(
        httpAddrToListenList,
        true,
        SocketFactory::nttDisabled ) );

    if( !m_multiAddressHttpServer->bind() )
        return 3;

    //TODO: #ak process privilege reduction should be made here

    if( !m_multiAddressHttpServer->listen() )
        return 5;

    //TODO #ak remove qt event loop
    //application's main loop
    const int result = application()->exec();

    //stopping accepting incoming connections
    m_multiAddressHttpServer.reset();

    return result;
}

void CloudDBProcess::start()
{
    QtSingleCoreApplication* application = this->application();

    if( application->isRunning() )
    {
        NX_LOG( "Server already started", cl_logERROR );
        application->quit();
        return;
    }
}

void CloudDBProcess::stop()
{
    application()->quit();
}

void CloudDBProcess::initializeLogging(
    const QString& dataLocation,
    const QString& logLevel )
{
    //logging
    if( logLevel != QString::fromLatin1("none") )
    {
        const QString& logDir = m_settings->value("logDir", dataLocation + QLatin1String("/log/")).toString();
        QDir().mkpath(logDir);
        const QString& logFileName = logDir + QLatin1String("/log_file");
        if( cl_log.create(logFileName, 1024 * 1024 * 10, 5, cl_logDEBUG1) )
            QnLog::initLog(logLevel);
        else
            std::wcerr << L"Failed to create log file " << logFileName.toStdWString() << std::endl;
        NX_LOG(lit("================================================================================="), cl_logALWAYS);
        NX_LOG(lit("%1 started").arg(QN_APPLICATION_NAME), cl_logALWAYS);
        NX_LOG(lit("Software version: %1").arg(QN_APPLICATION_VERSION), cl_logALWAYS);
        NX_LOG(lit("Software revision: %1").arg(QN_APPLICATION_REVISION), cl_logALWAYS);
    }
}

QString CloudDBProcess::getDataDirectory()
{
    const QString& dataDirFromSettings = m_settings->value( "dataDir" ).toString();
    if( !dataDirFromSettings.isEmpty() )
        return dataDirFromSettings;

#ifdef Q_OS_LINUX
    QString defVarDirName = QString("/opt/%1/%2/var").arg(VER_LINUX_ORGANIZATION_NAME).arg(VER_PRODUCTNAME_STR);
    QString varDirName = qSettings.value("varDir", defVarDirName).toString();
    return varDirName;
#else
    const QStringList& dataDirList = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
    return dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
}

int CloudDBProcess::printHelp()
{
    //TODO #ak

    return 0;
}
