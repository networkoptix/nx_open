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
#include <QtSql/QSqlQuery>

#include <api/global_settings.h>
#include <network/auth_restriction_list.h>
#include <utils/network/http/auth_tools.h>
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
#include "http_handlers/bind_system_handler.h"
#include "http_handlers/get_account_handler.h"
#include "http_handlers/verify_email_address_handler.h"
#include "managers/account_manager.h"
#include "managers/email_manager.h"
#include "managers/system_manager.h"

#include "version.h"


static int registerQtResources()
{
    Q_INIT_RESOURCE( libcloud_db );
    return 0;
}

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

    //if call Q_INIT_RESOURCE directly, linker will search for nx::cdb::libcloud_db and fail...
    registerQtResources();
}

void CloudDBProcess::pleaseStop()
{
    application()->quit();
}

int CloudDBProcess::executeApplication()
{
    try
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
    
        nx::db::DBManager dbManager(settings.dbConnectionOptions());
        if( !initializeDB(&dbManager) )
        {
            NX_LOG( lit("Failed to initialize DB connection"), cl_logALWAYS );
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
    
        QnAuthMethodRestrictionList authRestrictionList;
        authRestrictionList.allow( AddAccountHttpHandler::HANDLER_PATH, AuthMethod::noAuth );
        authRestrictionList.allow( VerifyEmailAddressHandler::HANDLER_PATH, AuthMethod::noAuth );
        AuthenticationManager authenticationManager( 
            accountManager,
            systemManager,
            authRestrictionList );
        httpServerManagers.setAuthenticationManager( &authenticationManager );

        AuthorizationManager authorizationManager;
    
        //registering HTTP handlers
        registerApiHandlers(
            &httpMessageDispatcher,
            authorizationManager,
            &accountManager,
            &systemManager );
    
        m_multiAddressHttpServer.reset( new MultiAddressServer<nx_http::HttpStreamSocketServer>(
            httpAddrToListenList,
            false,  //TODO #ak enable ssl when it works properly
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
    
        return result;
    }
    catch( const std::exception& e )
    {
        NX_LOG( lit("Failed to start application. %1").arg(e.what()), cl_logALWAYS );
        return 3;
    }
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
    const AuthorizationManager& authorizationManager,
    AccountManager* const accountManager,
    SystemManager* const systemManager )
{
    msgDispatcher->registerRequestProcessor<AddAccountHttpHandler>(
        AddAccountHttpHandler::HANDLER_PATH,
        [accountManager, &authorizationManager]() -> std::unique_ptr<AddAccountHttpHandler> {
            return std::make_unique<AddAccountHttpHandler>( accountManager, authorizationManager );
        } );

    msgDispatcher->registerRequestProcessor<VerifyEmailAddressHandler>(
        VerifyEmailAddressHandler::HANDLER_PATH,
        [accountManager, &authorizationManager]() -> std::unique_ptr<VerifyEmailAddressHandler> {
            return std::make_unique<VerifyEmailAddressHandler>( accountManager, authorizationManager );
        } );

    msgDispatcher->registerRequestProcessor<GetAccountHttpHandler>(
        GetAccountHttpHandler::HANDLER_PATH,
        [accountManager, &authorizationManager]() -> std::unique_ptr<GetAccountHttpHandler> {
            return std::make_unique<GetAccountHttpHandler>( accountManager, authorizationManager );
        } );

    msgDispatcher->registerRequestProcessor<BindSystemHandler>(
        BindSystemHandler::HANDLER_PATH,
        [systemManager, &authorizationManager]() -> std::unique_ptr<BindSystemHandler> {
            return std::make_unique<BindSystemHandler>( systemManager, authorizationManager );
        } );
}

bool CloudDBProcess::initializeDB( nx::db::DBManager* const dbManager )
{
    for (;;)
    {
        if (dbManager->init())
            break;
        NX_LOG(lit("Cannot start application due to DB connect error"), cl_logALWAYS);
        std::this_thread::sleep_for(
            std::chrono::seconds(DB_REPEATED_CONNECTION_ATTEMPT_DELAY_SEC));
    }

    if (!tuneDB(dbManager))
    {
        NX_LOG("Failed to tune DB", cl_logALWAYS);
        return false;
    }

    if (!updateDB(dbManager))
    {
        NX_LOG("Could not update DB to current vesion", cl_logALWAYS);
        return false;
    }

    return true;
}
//a3270856235d48e5f5af1fca6af673c7
bool CloudDBProcess::tuneDB( nx::db::DBManager* const dbManager )
{
    if( dbManager->connectionOptions().driverName != lit("QSQLITE") )
        return true;

    std::promise<nx::db::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();

    //starting async operation
    using namespace std::placeholders;
    dbManager->executeUpdateWithoutTran(
        [](QSqlDatabase* connection) ->nx::db::DBResult {
            QSqlQuery enableWalQuery(*connection);
            enableWalQuery.prepare("PRAGMA journal_mode = WAL");
            if (!enableWalQuery.exec())
                return nx::db::DBResult::ioError;

            QSqlQuery enableFKQuery(*connection);
            enableFKQuery.prepare("PRAGMA foreign_keys = ON");
            if (!enableFKQuery.exec())
                return nx::db::DBResult::ioError;

            return nx::db::DBResult::ok;
        },
        [&](nx::db::DBResult dbResult) {
            cacheFilledPromise.set_value(dbResult);
        });

    //waiting for completion
    future.wait();
    return future.get() == nx::db::DBResult::ok;
}

bool CloudDBProcess::updateDB( nx::db::DBManager* const dbManager )
{
    //updating DB structure to actual state
    nx::db::DBStructureUpdater dbStructureUpdater( dbManager );
    dbStructureUpdater.addUpdateScript( db::createAccountData );
    dbStructureUpdater.addUpdateScript( db::createSystemData );
    dbStructureUpdater.addUpdateScript( db::systemToAccountMapping );
    return dbStructureUpdater.updateStructSync();
}

}   //cdb
}   //nx
