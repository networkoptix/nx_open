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
#include <platform/process/current_process.h>
#include <nx/network/auth_restriction_list.h>
#include <nx/network/http/auth_tools.h>
#include <utils/common/cpp14.h>
#include <nx/utils/log/log.h>
#include <utils/common/systemerror.h>
#include <utils/db/db_structure_updater.h>
#include <nx/network/socket_global.h>
#include <nx/network/http/server/http_message_dispatcher.h>

#include "access_control/authentication_manager.h"
#include "db/structure_update_statements.h"
#include "http_handlers/activate_account_handler.h"
#include "http_handlers/add_account_handler.h"
#include "http_handlers/bind_system_handler.h"
#include "http_handlers/get_account_handler.h"
#include "http_handlers/update_account_handler.h"
#include "http_handlers/get_cloud_users_of_system.h"
#include "http_handlers/get_systems_handler.h"
#include "http_handlers/get_cdb_nonce_handler.h"
#include "http_handlers/get_authentication_response_handler.h"
#include "http_handlers/ping.h"
#include "http_handlers/unbind_system_handler.h"
#include "http_handlers/share_system_handler.h"
#include "http_handlers/update_system_sharing_handler.h"
#include "managers/account_manager.h"
#include "managers/auth_provider.h"
#include "managers/email_manager.h"
#include "managers/system_manager.h"
#include "stree/stree_manager.h"

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
    m_argv( argv ),
    m_terminated( false ),
    m_timerID( -1 )
{
    setServiceDescription(QN_APPLICATION_NAME);

    //if call Q_INIT_RESOURCE directly, linker will search for nx::cdb::libcloud_db and fail...
    registerQtResources();
}

void CloudDBProcess::pleaseStop()
{
    m_terminated = true;
    if (application())
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
    
        EMailManager emailManager(settings);
        StreeManager streeManager(settings.auth());

        //creating data managers
        AccountManager accountManager(
            settings,
            &dbManager,
            &emailManager);
    
        SystemManager systemManager(
            accountManager,
            &dbManager);
    
        QnAuthMethodRestrictionList authRestrictionList;
        authRestrictionList.allow( PingHandler::HANDLER_PATH, AuthMethod::noAuth );
        authRestrictionList.allow( AddAccountHttpHandler::HANDLER_PATH, AuthMethod::noAuth );
        authRestrictionList.allow( ActivateAccountHandler::HANDLER_PATH, AuthMethod::noAuth );

        AuthenticationManager authenticationManager( 
            accountManager,
            systemManager,
            authRestrictionList,
            streeManager);

        AuthorizationManager authorizationManager(
            streeManager,
            accountManager,
            systemManager);
    
        AuthenticationProvider authProvider(
            accountManager,
            systemManager);

        nx_http::MessageDispatcher httpMessageDispatcher;
        //registering HTTP handlers
        registerApiHandlers(
            &httpMessageDispatcher,
            authorizationManager,
            &accountManager,
            &systemManager,
            &authProvider);
    
        MultiAddressServer<nx_http::HttpStreamSocketServer> multiAddressHttpServer(
            &authenticationManager,
            &httpMessageDispatcher,
            false,  //TODO #ak enable ssl when it works properly
            SocketFactory::NatTraversalType::nttDisabled );

        if( !multiAddressHttpServer.bind(httpAddrToListenList) )
            return 3;

        // process privilege reduction
        CurrentProcess::changeUser( settings.changeUser() );
    
        if( !multiAddressHttpServer.listen() )
            return 5;

        application()->installEventFilter(this);
        if (m_terminated)
            return 0;
    
        NX_LOG( lit( "%1 has been started" ).arg(QN_APPLICATION_NAME), cl_logALWAYS );

        //TODO #ak remove qt event loop
        //application's main loop

        //starting timer to check for m_terminated again after event loop start
        m_timerID = application()->startTimer(0);
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
    pleaseStop();
    //TODO #ak wait for executeApplication to return?
}

bool CloudDBProcess::eventFilter(QObject* /*watched*/, QEvent* /*event*/)
{
    if (m_timerID != -1)
    {
        application()->killTimer(m_timerID);
        m_timerID = -1;
    }

    if (m_terminated)
        application()->quit();
    return false;
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
    SystemManager* const systemManager,
    AuthenticationProvider* const authProvider)
{
    msgDispatcher->registerRequestProcessor<PingHandler>(
        PingHandler::HANDLER_PATH,
        [&authorizationManager]() -> std::unique_ptr<PingHandler> {
            return std::make_unique<PingHandler>( authorizationManager );
        } );

    //accounts
    msgDispatcher->registerRequestProcessor<AddAccountHttpHandler>(
        AddAccountHttpHandler::HANDLER_PATH,
        [accountManager, &authorizationManager]() -> std::unique_ptr<AddAccountHttpHandler> {
            return std::make_unique<AddAccountHttpHandler>( accountManager, authorizationManager );
        } );

    msgDispatcher->registerRequestProcessor<ActivateAccountHandler>(
        ActivateAccountHandler::HANDLER_PATH,
        [accountManager, &authorizationManager]() -> std::unique_ptr<ActivateAccountHandler> {
            return std::make_unique<ActivateAccountHandler>( accountManager, authorizationManager );
        } );

    msgDispatcher->registerRequestProcessor<GetAccountHttpHandler>(
        GetAccountHttpHandler::HANDLER_PATH,
        [accountManager, &authorizationManager]() -> std::unique_ptr<GetAccountHttpHandler> {
            return std::make_unique<GetAccountHttpHandler>( accountManager, authorizationManager );
        } );

    msgDispatcher->registerRequestProcessor<UpdateAccountHttpHandler>(
        UpdateAccountHttpHandler::HANDLER_PATH,
        [accountManager, &authorizationManager]() -> std::unique_ptr<UpdateAccountHttpHandler> {
            return std::make_unique<UpdateAccountHttpHandler>( accountManager, authorizationManager );
        } );

    //systems
    msgDispatcher->registerRequestProcessor<BindSystemHandler>(
        BindSystemHandler::HANDLER_PATH,
        [systemManager, &authorizationManager]() -> std::unique_ptr<BindSystemHandler> {
            return std::make_unique<BindSystemHandler>( systemManager, authorizationManager );
        } );

    msgDispatcher->registerRequestProcessor<GetSystemsHandler>(
        GetSystemsHandler::HANDLER_PATH,
        [systemManager, &authorizationManager]() -> std::unique_ptr<GetSystemsHandler> {
            return std::make_unique<GetSystemsHandler>( systemManager, authorizationManager );
        } );

    msgDispatcher->registerRequestProcessor<UnbindSystemHandler>(
        UnbindSystemHandler::HANDLER_PATH,
        [systemManager, &authorizationManager]() -> std::unique_ptr<UnbindSystemHandler> {
            return std::make_unique<UnbindSystemHandler>( systemManager, authorizationManager );
        } );

    msgDispatcher->registerRequestProcessor<ShareSystemHttpHandler>(
        ShareSystemHttpHandler::HANDLER_PATH,
        [systemManager, &authorizationManager]() -> std::unique_ptr<ShareSystemHttpHandler> {
            return std::make_unique<ShareSystemHttpHandler>(systemManager, authorizationManager);
        } );

    msgDispatcher->registerRequestProcessor<UpdateSystemSharingHandler>(
        UpdateSystemSharingHandler::HANDLER_PATH,
        [systemManager, &authorizationManager]() -> std::unique_ptr<UpdateSystemSharingHandler> {
            return std::make_unique<UpdateSystemSharingHandler>(systemManager, authorizationManager);
        });

    msgDispatcher->registerRequestProcessor<GetCloudUsersOfSystemHandler>(
        GetCloudUsersOfSystemHandler::HANDLER_PATH,
        [systemManager, &authorizationManager]() -> std::unique_ptr<GetCloudUsersOfSystemHandler> {
            return std::make_unique<GetCloudUsersOfSystemHandler>(systemManager, authorizationManager);
        } );

    //authentication
    msgDispatcher->registerRequestProcessor<GetCdbNonceHandler>(
        GetCdbNonceHandler::HANDLER_PATH,
        [authProvider, &authorizationManager]() -> std::unique_ptr<GetCdbNonceHandler> {
            return std::make_unique<GetCdbNonceHandler>( authProvider, authorizationManager );
        } );

    msgDispatcher->registerRequestProcessor<GetAuthenticationResponseHandler>(
        GetAuthenticationResponseHandler::HANDLER_PATH,
        [authProvider, &authorizationManager]() -> std::unique_ptr<GetAuthenticationResponseHandler> {
            return std::make_unique<GetAuthenticationResponseHandler>( authProvider, authorizationManager );
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

    if (!configureDB(dbManager))
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

bool CloudDBProcess::configureDB( nx::db::DBManager* const dbManager )
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
            {
                NX_LOG(lit("sqlite configure. Failed to enable WAL mode. %1").
                    arg(connection->lastError().text()), cl_logWARNING);
                return nx::db::DBResult::ioError;
            }

            QSqlQuery enableFKQuery(*connection);
            enableFKQuery.prepare("PRAGMA foreign_keys = ON");
            if (!enableFKQuery.exec())
            {
                NX_LOG(lit("sqlite configure. Failed to enable foreign keys. %1").
                    arg(connection->lastError().text()), cl_logWARNING);
                return nx::db::DBResult::ioError;
            }

            return nx::db::DBResult::ok;
        },
        [&](nx::db::DBResult dbResult) {
            cacheFilledPromise.set_value(dbResult);
        });

    //waiting for completion
    future.wait();
    return future.get() == nx::db::DBResult::ok;
}

bool CloudDBProcess::updateDB(nx::db::DBManager* const dbManager)
{
    //updating DB structure to actual state
    nx::db::DBStructureUpdater dbStructureUpdater(dbManager);
    dbStructureUpdater.addUpdateScript(db::createAccountData);
    dbStructureUpdater.addUpdateScript(db::createSystemData);
    dbStructureUpdater.addUpdateScript(db::systemToAccountMapping);
    dbStructureUpdater.addUpdateScript(db::addCustomizationToSystem);
    dbStructureUpdater.addUpdateScript(db::addCustomizationToAccount);
    return dbStructureUpdater.updateStructSync();
}

}   //cdb
}   //nx
