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
#include <utils/common/guard.h>
#include <nx/network/socket_global.h>
#include <nx/network/http/server/http_message_dispatcher.h>

#include <cloud_db_client/src/cdb_request_path.h>

#include "access_control/authentication_manager.h"
#include "db/structure_update_statements.h"
#include "http_handlers/bind_system_handler.h"
#include "http_handlers/get_access_role_list.h"
#include "http_handlers/get_cloud_users_of_system.h"
#include "http_handlers/get_systems_handler.h"
#include "http_handlers/update_system_name_handler.h"
#include "http_handlers/get_cdb_nonce_handler.h"
#include "http_handlers/get_authentication_response_handler.h"
#include "http_handlers/ping.h"
#include "http_handlers/unbind_system_handler.h"
#include "http_handlers/share_system_handler.h"
#include "managers/account_manager.h"
#include "managers/auth_provider.h"
#include "managers/email_manager.h"
#include "managers/event_manager.h"
#include "managers/system_manager.h"
#include "managers/temporary_account_password_manager.h"
#include "stree/stree_manager.h"

#include <libcloud_db_app_info.h>
#include <utils/common/app_info.h>


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
#ifdef USE_QAPPLICATION
    QtService<QtSingleCoreApplication>(argc, argv, QnLibCloudDbAppInfo::applicationName()),
#endif
    m_argc( argc ),
    m_argv( argv ),
    m_terminated( false ),
    m_timerID( -1 ),
    m_httpMessageDispatcher( nullptr ),
    m_authorizationManager( nullptr )
{
#ifdef USE_QAPPLICATION
    setServiceDescription(QnLibCloudDbAppInfo::applicationDisplayName());
#endif

    //if call Q_INIT_RESOURCE directly, linker will search for nx::cdb::libcloud_db and fail...
    registerQtResources();
}

void CloudDBProcess::pleaseStop()
{
#ifdef USE_QAPPLICATION
    m_terminated = true;
    if (application())
        application()->quit();
#else
    m_processTerminationEvent.set_value();
#endif
}

void CloudDBProcess::setOnStartedEventHandler(
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler)
{
    m_startedEventHandler = std::move(handler);
}

const std::vector<SocketAddress>& CloudDBProcess::httpEndpoints() const
{
    return m_httpEndpoints;
}

#ifdef USE_QAPPLICATION
int CloudDBProcess::executeApplication()
#else
int CloudDBProcess::exec()
#endif
{
    bool processStartResult = false;
    auto triggerOnStartedEventHandlerGuard = makeScopedGuard(
        [this, &processStartResult]
        {
            if (m_startedEventHandler)
                m_startedEventHandler(processStartResult);
        });

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

        nx::db::AsyncSqlQueryExecutor dbManager(settings.dbConnectionOptions());
        if( !initializeDB(&dbManager) )
        {
            NX_LOG( lit("Failed to initialize DB connection"), cl_logALWAYS );
            return 2;
        }

        nx::utils::TimerManager timerManager;
        timerManager.start();

        std::unique_ptr<AbstractEmailManager> emailManager(
            EMailManagerFactory::create(settings));

        CdbAttrNameSet cdbAttrNameSet;
        StreeManager streeManager(
            cdbAttrNameSet,
            settings.auth().rulesXmlPath);

        nx_http::MessageDispatcher httpMessageDispatcher;
        m_httpMessageDispatcher = &httpMessageDispatcher;

        //creating data managers
        TemporaryAccountPasswordManager tempPasswordManager(
            settings,
            &dbManager);

        AccountManager accountManager(
            settings,
            &tempPasswordManager,
            &dbManager,
            emailManager.get());

        EventManager eventManager(settings);

        SystemManager systemManager(
            settings,
            &timerManager,
            accountManager,
            eventManager,
            &dbManager);

        //TODO #ak move following to stree xml
        QnAuthMethodRestrictionList authRestrictionList;
        authRestrictionList.allow(PingHandler::kHandlerPath, AuthMethod::noAuth);
        authRestrictionList.allow(kAccountRegisterPath, AuthMethod::noAuth);
        authRestrictionList.allow(kAccountActivatePath, AuthMethod::noAuth);
        authRestrictionList.allow(kAccountReactivatePath, AuthMethod::noAuth);

        std::vector<AbstractAuthenticationDataProvider*> authDataProviders;
        authDataProviders.push_back(&accountManager);
        authDataProviders.push_back(&systemManager);
        AuthenticationManager authenticationManager(
            std::move(authDataProviders),
            authRestrictionList,
            streeManager);

        AuthorizationManager authorizationManager(
            streeManager,
            accountManager,
            systemManager);
        m_authorizationManager = &authorizationManager;

        AuthenticationProvider authProvider(
            settings,
            accountManager,
            systemManager);

        //registering HTTP handlers
        registerApiHandlers(
            &httpMessageDispatcher,
            authorizationManager,
            &accountManager,
            &systemManager,
            &authProvider);
        eventManager.registerHttpHandlers(
            authorizationManager,
            &httpMessageDispatcher);

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
        m_httpEndpoints = multiAddressHttpServer.endpoints();

#ifdef USE_QAPPLICATION
        application()->installEventFilter(this);
#endif
        if (m_terminated)
            return 0;

        NX_LOG(lit("%1 has been started")
                .arg(QnLibCloudDbAppInfo::applicationDisplayName()),
               cl_logALWAYS);
        //std::cout << QnLibCloudDbAppInfo::applicationDisplayName().toStdString()
        //    << " has been started" << std::endl;

        processStartResult = true;
        triggerOnStartedEventHandlerGuard.fire();

#ifdef USE_QAPPLICATION
        //starting timer to check for m_terminated again after event loop start
        m_timerID = application()->startTimer(0);

        //TODO #ak remove qt event loop
        //application's main loop
        const int result = application()->exec();
        return result;
#else
        m_processTerminationEvent.get_future().wait();
        return 0;
#endif
    }
    catch( const std::exception& e )
    {
        NX_LOG( lit("Failed to start application. %1").arg(e.what()), cl_logALWAYS );
        return 3;
    }
}

#ifdef USE_QAPPLICATION
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
#endif

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
        NX_LOG(lit("%1 started").arg(QnLibCloudDbAppInfo::applicationDisplayName()), cl_logALWAYS);
        NX_LOG(lit("Software version: %1").arg(QnAppInfo::applicationVersion()), cl_logALWAYS);
        NX_LOG(lit("Software revision: %1").arg(QnAppInfo::applicationRevision()), cl_logALWAYS);
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
        PingHandler::kHandlerPath,
        [&authorizationManager]() -> std::unique_ptr<PingHandler>
        {
            return std::make_unique<PingHandler>(authorizationManager);
        });

    //accounts
    registerHttpHandler(
        kAccountRegisterPath,
        &AccountManager::addAccount, accountManager,
        EntityType::account, DataActionType::insert);

    registerHttpHandler(
        kAccountActivatePath,
        &AccountManager::activate, accountManager,
        EntityType::account, DataActionType::update);

    registerHttpHandler(
        kAccountGetPath,
        &AccountManager::getAccount, accountManager,
        EntityType::account, DataActionType::fetch);

    registerHttpHandler(
        kAccountUpdatePath,
        &AccountManager::updateAccount, accountManager,
        EntityType::account, DataActionType::update);

    registerHttpHandler(
        kAccountPasswordResetPath,
        &AccountManager::resetPassword, accountManager,
        EntityType::account, DataActionType::update);

    registerHttpHandler(
        kAccountReactivatePath,
        &AccountManager::reactivateAccount, accountManager,
        EntityType::account, DataActionType::update);

    registerHttpHandler(
        kAccountCreateTemporaryCredentialsPath,
        &AccountManager::createTemporaryCredentials, accountManager,
        EntityType::account, DataActionType::update);

    //systems
    msgDispatcher->registerRequestProcessor<BindSystemHandler>(
        BindSystemHandler::kHandlerPath,
        [systemManager, &authorizationManager]() -> std::unique_ptr<BindSystemHandler> {
            return std::make_unique<BindSystemHandler>( systemManager, authorizationManager );
        } );

    msgDispatcher->registerRequestProcessor<GetSystemsHandler>(
        GetSystemsHandler::kHandlerPath,
        [systemManager, &authorizationManager]() -> std::unique_ptr<GetSystemsHandler> {
            return std::make_unique<GetSystemsHandler>( systemManager, authorizationManager );
        } );

    msgDispatcher->registerRequestProcessor<UnbindSystemHandler>(
        UnbindSystemHandler::kHandlerPath,
        [systemManager, &authorizationManager]() -> std::unique_ptr<UnbindSystemHandler> {
            return std::make_unique<UnbindSystemHandler>( systemManager, authorizationManager );
        } );

    msgDispatcher->registerRequestProcessor<ShareSystemHttpHandler>(
        ShareSystemHttpHandler::kHandlerPath,
        [systemManager, &authorizationManager]() -> std::unique_ptr<ShareSystemHttpHandler> {
            return std::make_unique<ShareSystemHttpHandler>(systemManager, authorizationManager);
        } );

    msgDispatcher->registerRequestProcessor<GetCloudUsersOfSystemHandler>(
        GetCloudUsersOfSystemHandler::kHandlerPath,
        [systemManager, &authorizationManager]() -> std::unique_ptr<GetCloudUsersOfSystemHandler> {
            return std::make_unique<GetCloudUsersOfSystemHandler>(systemManager, authorizationManager);
        } );

    msgDispatcher->registerRequestProcessor<GetAccessRoleListHandler>(
        GetAccessRoleListHandler::kHandlerPath,
        [systemManager, &authorizationManager]() -> std::unique_ptr<GetAccessRoleListHandler> {
            return std::make_unique<GetAccessRoleListHandler>(systemManager, authorizationManager);
        });

    msgDispatcher->registerRequestProcessor<UpdateSystemNameHttpHandler>(
        UpdateSystemNameHttpHandler::kHandlerPath,
        [systemManager, &authorizationManager]() -> std::unique_ptr<UpdateSystemNameHttpHandler> {
            return std::make_unique<UpdateSystemNameHttpHandler>(systemManager, authorizationManager);
        });

    //authentication
    msgDispatcher->registerRequestProcessor<GetCdbNonceHandler>(
        GetCdbNonceHandler::kHandlerPath,
        [authProvider, &authorizationManager]() -> std::unique_ptr<GetCdbNonceHandler> {
            return std::make_unique<GetCdbNonceHandler>( authProvider, authorizationManager );
        } );

    msgDispatcher->registerRequestProcessor<GetAuthenticationResponseHandler>(
        GetAuthenticationResponseHandler::kHandlerPath,
        [authProvider, &authorizationManager]() -> std::unique_ptr<GetAuthenticationResponseHandler> {
            return std::make_unique<GetAuthenticationResponseHandler>( authProvider, authorizationManager );
        } );
}

bool CloudDBProcess::initializeDB( nx::db::AsyncSqlQueryExecutor* const dbManager )
{
    for (;;)
    {
        if (dbManager->init())
            break;
        NX_LOG(lit("Cannot start application due to DB connect error"), cl_logALWAYS);
        std::this_thread::sleep_for(
            std::chrono::seconds(DB_REPEATED_CONNECTION_ATTEMPT_DELAY_SEC));
    }

    if (!updateDB(dbManager))
    {
        NX_LOG("Could not update DB to current vesion", cl_logALWAYS);
        return false;
    }

    if (!configureDB(dbManager))
    {
        NX_LOG("Failed to tune DB", cl_logALWAYS);
        return false;
    }

    return true;
}

bool CloudDBProcess::configureDB( nx::db::AsyncSqlQueryExecutor* const dbManager )
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

            //QSqlQuery setLockingModeQuery(*connection);
            //setLockingModeQuery.prepare("PRAGMA locking_mode = NORMAL");
            //if (!setLockingModeQuery.exec())
            //{
            //    NX_LOG(lit("sqlite configure. Failed to set locking mode. %1").
            //        arg(connection->lastError().text()), cl_logWARNING);
            //    return nx::db::DBResult::ioError;
            //}

            return nx::db::DBResult::ok;
        },
        [&](nx::db::DBResult dbResult) {
            cacheFilledPromise.set_value(dbResult);
        });

    //waiting for completion
    future.wait();
    return future.get() == nx::db::DBResult::ok;
}

bool CloudDBProcess::updateDB(nx::db::AsyncSqlQueryExecutor* const dbManager)
{
    //updating DB structure to actual state
    nx::db::DBStructureUpdater dbStructureUpdater(dbManager);
    dbStructureUpdater.addFullSchemaScript(13, db::kCreateDbVersion13);

    dbStructureUpdater.addUpdateScript(db::kCreateAccountData);
    dbStructureUpdater.addUpdateScript(db::kCreateSystemData);
    dbStructureUpdater.addUpdateScript(db::kSystemToAccountMapping);
    dbStructureUpdater.addUpdateScript(db::kAddCustomizationToSystem);
    dbStructureUpdater.addUpdateScript(db::kAddCustomizationToAccount);
    dbStructureUpdater.addUpdateScript(db::kAddTemporaryAccountPassword);
    dbStructureUpdater.addUpdateScript(db::kAddIsEmailCodeToTemporaryAccountPassword);
    dbStructureUpdater.addUpdateScript(db::kRenameSystemAccessRoles);
    dbStructureUpdater.addUpdateScript(db::kChangeSystemIdTypeToString);
    dbStructureUpdater.addUpdateScript(db::kAddDeletedSystemState);
    dbStructureUpdater.addUpdateScript(db::kSystemExpirationTime);
    dbStructureUpdater.addUpdateScript(db::kReplaceBlobWithVarchar);
    return dbStructureUpdater.updateStructSync();
}

template<typename InputData, typename OutputData, typename ManagerType>
void CloudDBProcess::registerHttpHandler(
    const char* handlerPath,
    void (ManagerType::*managerFunc)(
        const AuthorizationInfo& authzInfo,
        InputData inputData,
        std::function<void(api::ResultCode, OutputData)> completionHandler),
    ManagerType* manager,
    EntityType entityType,
    DataActionType dataActionType)
{
    typedef AbstractFiniteMsgBodyHttpHandler<InputData, OutputData> HttpHandlerType;

    m_httpMessageDispatcher->registerRequestProcessor<HttpHandlerType>(
        handlerPath,
        [this, managerFunc, manager, entityType, dataActionType]()
            -> std::unique_ptr<HttpHandlerType>
        {
            using namespace std::placeholders;
            return std::make_unique<HttpHandlerType>(
                entityType,
                dataActionType,
                *m_authorizationManager,
                std::bind(managerFunc, manager, _1, _2, _3));
        });
}

template<typename InputData, typename ManagerType>
void CloudDBProcess::registerHttpHandler(
    const char* handlerPath,
    void (ManagerType::*managerFunc)(
        const AuthorizationInfo& authzInfo,
        InputData inputData,
        std::function<void(api::ResultCode)> completionHandler),
    ManagerType* manager,
    EntityType entityType,
    DataActionType dataActionType)
{
    typedef AbstractFiniteMsgBodyHttpHandler<InputData> HttpHandlerType;

    m_httpMessageDispatcher->registerRequestProcessor<HttpHandlerType>(
        handlerPath,
        [this, managerFunc, manager, entityType, dataActionType]()
            -> std::unique_ptr<HttpHandlerType>
        {
            using namespace std::placeholders;
            return std::make_unique<HttpHandlerType>(
                entityType,
                dataActionType,
                *m_authorizationManager,
                std::bind(managerFunc, manager, _1, _2, _3));
        });
}

template<typename OutputData, typename ManagerType>
void CloudDBProcess::registerHttpHandler(
    const char* handlerPath,
    void (ManagerType::*managerFunc)(
        const AuthorizationInfo& authzInfo,
        std::function<void(api::ResultCode, OutputData)> completionHandler),
    ManagerType* manager,
    EntityType entityType,
    DataActionType dataActionType)
{
    typedef AbstractFiniteMsgBodyHttpHandler<void, OutputData> HttpHandlerType;

    m_httpMessageDispatcher->registerRequestProcessor<HttpHandlerType>(
        handlerPath,
        [this, managerFunc, manager, entityType, dataActionType]()
            -> std::unique_ptr<HttpHandlerType>
        {
            using namespace std::placeholders;
            return std::make_unique<HttpHandlerType>(
                entityType,
                dataActionType,
                *m_authorizationManager,
                std::bind(managerFunc, manager, _1, _2));
        });
}

template<typename ManagerType>
void CloudDBProcess::registerHttpHandler(
    const char* handlerPath,
    void (ManagerType::*managerFunc)(
        const AuthorizationInfo& authzInfo,
        std::function<void(api::ResultCode)> completionHandler),
    ManagerType* manager,
    EntityType entityType,
    DataActionType dataActionType)
{
    typedef AbstractFiniteMsgBodyHttpHandler<void, void> HttpHandlerType;

    m_httpMessageDispatcher->registerRequestProcessor<HttpHandlerType>(
        handlerPath,
        [this, managerFunc, manager, entityType, dataActionType]()
            -> std::unique_ptr<HttpHandlerType>
        {
            using namespace std::placeholders;
            return std::make_unique<HttpHandlerType>(
                entityType,
                dataActionType,
                *m_authorizationManager,
                std::bind(managerFunc, manager, _1, _2));
        });
}

}   //cdb
}   //nx
