/**********************************************************
* 8 may 2015
* a.kolesnikov
***********************************************************/

#include "cloud_db_process.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <list>
#include <thread>

#include <QtCore/QDir>

#include <utils/common/cpp14.h>
#include <utils/common/log.h>
#include <utils/common/systemerror.h>
#include <utils/db/db_structure_updater.h>
#include <utils/network/aio/aioservice.h>
#include <utils/network/http/server/http_message_dispatcher.h>
#include <utils/network/http/server/server_managers.h>

#include "access_control/authentication_manager.h"
#include "db/structure_update_statements.h"
#include "http_handlers/add_account_handler.h"
#include "managers/account_manager.h"
#include "managers/email_manager.h"
#include "managers/system_manager.h"

#include "version.h"


namespace nx {
namespace cdb {

static const int DB_REPEATED_CONNECTION_ATTEMPT_DELAY_SEC = 5;

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
    //reading settings
    conf::Settings settings;
    //parsing command line arguments
    settings.load( m_argc, m_argv );
    if( settings.showHelp() )
    {
        settings.printCmdLineArgsHelp();
        return 0;
    }

    initializeLogging( settings );

    const auto& httpAddrToListenList = settings.endpointsToListen();
    if( httpAddrToListenList.empty() )
    {
        NX_LOG( "No HTTP address to listen", cl_logALWAYS );
        return 1;
    }

    nx::db::DBManager dbManager( settings.dbConnectionOptions() );
    for( ;; )
    {
        if( dbManager.init() )
            break;
        NX_LOG( lit("Cannot start application due to DB connect error"), cl_logALWAYS );
        std::this_thread::sleep_for( std::chrono::seconds( DB_REPEATED_CONNECTION_ATTEMPT_DELAY_SEC ) );
    }

    if( !updateDB( &dbManager ) )
    {
        NX_LOG( "Could not update DB to current vesion", cl_logALWAYS );
        return 2;
    }

    EMailManager emailManager( settings );

    //creating data managers
    AccountManager accountManager(
        settings,
        &dbManager,
        &emailManager );

    SystemManager systemManager( &dbManager );

    //contains singletones common for http server
    nx_http::ServerManagers httpServerManagers;

    nx_http::MessageDispatcher httpMessageDispatcher;
    httpServerManagers.setDispatcher( &httpMessageDispatcher );

    AuthenticationManager authenticationManager;
    httpServerManagers.setAuthenticationManager( &authenticationManager );

    //registering HTTP handlers
    registerApiHandlers(
        &httpMessageDispatcher,
        &accountManager,
        &systemManager );

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

    NX_LOG( lit( "%1 has been started" ).arg(QN_APPLICATION_NAME), cl_logALWAYS );

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

void CloudDBProcess::initializeLogging( const conf::Settings& settings )
{
    //logging
    if( settings.logging().logLevel != QString::fromLatin1("none") )
    {
        const QString& logDir = settings.logging().logDir;
            
        QDir().mkpath(logDir);
        const QString& logFileName = logDir + lit("/log_file");
        if( cl_log.create(logFileName, 1024 * 1024 * 10, 5, cl_logDEBUG1) )
            QnLog::initLog( settings.logging().logLevel );
        else
            std::wcerr << L"Failed to create log file " << logFileName.toStdWString() << std::endl;
        NX_LOG(lit("================================================================================="), cl_logALWAYS);
        NX_LOG(lit("%1 started").arg(QN_APPLICATION_NAME), cl_logALWAYS);
        NX_LOG(lit("Software version: %1").arg(QN_APPLICATION_VERSION), cl_logALWAYS);
        NX_LOG(lit("Software revision: %1").arg(QN_APPLICATION_REVISION), cl_logALWAYS);
    }
}

void CloudDBProcess::registerApiHandlers(
    nx_http::MessageDispatcher* const msgDispatcher,
    AccountManager* const accountManager,
    SystemManager* const /*systemManager*/ )
{
    msgDispatcher->registerRequestProcessor<cdb::AddAccountHttpHandler>(
        cdb::AddAccountHttpHandler::HANDLER_PATH,
        [accountManager]() -> cdb::AddAccountHttpHandler* {
            return new cdb::AddAccountHttpHandler( accountManager );
        } );
}

bool CloudDBProcess::updateDB( nx::db::DBManager* const dbManager )
{
    //updating DB structure to actual state
    nx::db::DBStructureUpdater dbStructureUpdater( dbManager );
    dbStructureUpdater.addUpdateScript( db::createAccountData );
    return dbStructureUpdater.updateStructSync();
}

}   //cdb
}   //nx
