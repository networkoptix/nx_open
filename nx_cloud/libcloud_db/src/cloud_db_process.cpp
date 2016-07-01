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
#include <type_traits>

#include <QtCore/QDir>
#include <QtSql/QSqlQuery>

#include <api/global_settings.h>
#include <platform/process/current_process.h>
#include <nx/network/auth_restriction_list.h>
#include <nx/network/http/auth_tools.h>
#include <utils/common/cpp14.h>
#include <nx/utils/log/log.h>
#include <nx/utils/type_utils.h>
#include <utils/common/systemerror.h>
#include <utils/db/db_structure_updater.h>
#include <utils/common/guard.h>
#include <nx/network/socket_global.h>
#include <nx/network/http/server/http_message_dispatcher.h>

#include <cloud_db_client/src/cdb_request_path.h>

#include "access_control/authentication_manager.h"
#include "db/structure_update_statements.h"
#include "http_handlers/ping.h"
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
    m_argc(argc),
    m_argv(argv),
    m_terminated(false),
    m_timerID(-1),
    m_settings(nullptr),
    m_dbManager(nullptr),
    m_timerManager(nullptr),
    m_emailManager(nullptr),
    m_streeManager(nullptr),
    m_httpMessageDispatcher(nullptr),
    m_tempPasswordManager(nullptr),
    m_accountManager(nullptr),
    m_eventManager(nullptr),
    m_systemManager(nullptr),
    m_authenticationManager(nullptr),
    m_authorizationManager(nullptr),
    m_authProvider(nullptr)
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
        m_settings = &settings;
        if( httpAddrToListenList.empty() )
        {
            NX_LOG( "No HTTP address to listen", cl_logALWAYS );
            return 1;
        }

        nx::db::AsyncSqlQueryExecutor dbManager(settings.dbConnectionOptions());
        m_dbManager = &dbManager;
        if( !initializeDB(&dbManager) )
        {
            NX_LOG( lit("Failed to initialize DB connection"), cl_logALWAYS );
            return 2;
        }

        nx::utils::TimerManager timerManager;
        m_timerManager = &timerManager;
        timerManager.start();

        std::unique_ptr<AbstractEmailManager> emailManager(
            EMailManagerFactory::create(settings));
        m_emailManager = emailManager.get();

        CdbAttrNameSet cdbAttrNameSet;
        StreeManager streeManager(
            cdbAttrNameSet,
            settings.auth().rulesXmlPath);
        m_streeManager = &streeManager;

        nx_http::MessageDispatcher httpMessageDispatcher;
        m_httpMessageDispatcher = &httpMessageDispatcher;

        //creating data managers
        TemporaryAccountPasswordManager tempPasswordManager(
            settings,
            &dbManager);
        m_tempPasswordManager = &tempPasswordManager;

        AccountManager accountManager(
            settings,
            streeManager,
            &tempPasswordManager,
            &dbManager,
            emailManager.get());
        m_accountManager = &accountManager;

        EventManager eventManager(settings);
        m_eventManager = &eventManager;

        SystemManager systemManager(
            settings,
            &timerManager,
            accountManager,
            eventManager,
            &dbManager);
        m_systemManager = &systemManager;

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
        m_authenticationManager = &authenticationManager;

        AuthorizationManager authorizationManager(
            streeManager,
            accountManager,
            systemManager);
        m_authorizationManager = &authorizationManager;

        AuthenticationProvider authProvider(
            settings,
            accountManager,
            systemManager);
        m_authProvider = &authProvider;

        //registering HTTP handlers
        registerApiHandlers(
            &httpMessageDispatcher,
            authorizationManager,
            &accountManager,
            &systemManager,
            &authProvider,
            &eventManager);
        //TODO #ak remove eventManager.registerHttpHandlers and register in registerApiHandlers
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
    AuthenticationProvider* const authProvider,
    EventManager* const /*eventManager*/)
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
    registerHttpHandler(
        kSystemBindPath,
        &SystemManager::bindSystemToAccount, systemManager,
        EntityType::system, DataActionType::insert);

    registerHttpHandler(
        kSystemGetPath,
        &SystemManager::getSystems, systemManager,
        EntityType::system, DataActionType::fetch);

    registerHttpHandler(
        kSystemUnbindPath,
        &SystemManager::unbindSystem, systemManager,
        EntityType::system, DataActionType::_delete);

    registerHttpHandler(
        kSystemSharePath,
        &SystemManager::shareSystem, systemManager,
        EntityType::system, DataActionType::update);

    registerHttpHandler(
        kSystemGetCloudUsersPath,
        &SystemManager::getCloudUsersOfSystem, systemManager,
        EntityType::system, DataActionType::fetch);

    registerHttpHandler(
        kSystemGetAccessRoleListPath,
        &SystemManager::getAccessRoleList, systemManager,
        EntityType::system, DataActionType::fetch);

    registerHttpHandler(
        kSystemUpdateSystemNamePath,
        &SystemManager::updateSystemName, systemManager,
        EntityType::system, DataActionType::update);

    //authentication
    registerHttpHandler(
        kAuthGetNoncePath,
        &AuthenticationProvider::getCdbNonce, authProvider,
        EntityType::account, DataActionType::fetch);

    registerHttpHandler(
        kAuthGetAuthenticationPath,
        &AuthenticationProvider::getAuthenticationResponse, authProvider,
        EntityType::account, DataActionType::fetch);
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
    dbStructureUpdater.addUpdateScript(db::kTemporaryAccountCredentials);
    dbStructureUpdater.addUpdateScript(db::kTemporaryAccountCredentialsProlongationPeriod);
    dbStructureUpdater.addUpdateScript(db::kAddCustomAndDisabledAccessRoles);
    return dbStructureUpdater.updateStructSync();
}

template<typename ManagerType, typename InputData, typename... OutputData>
void CloudDBProcess::registerHttpHandler(
    const char* handlerPath,
    void (ManagerType::*managerFunc)(
        const AuthorizationInfo& authzInfo,
        InputData inputData,
        std::function<void(api::ResultCode, OutputData...)> completionHandler),
    ManagerType* manager,
    EntityType entityType,
    DataActionType dataActionType)
{
    typedef typename nx::utils::tuple_first_element<void, std::tuple<OutputData...>>::type
        ActualOutputDataType;

    typedef AbstractFiniteMsgBodyHttpHandler<
        typename std::remove_const<typename std::remove_reference<InputData>::type>::type,
        typename std::remove_const<typename std::remove_reference<ActualOutputDataType>::type>::type
    > HttpHandlerType;

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

template<typename ManagerType, typename... OutputData>
void CloudDBProcess::registerHttpHandler(
    const char* handlerPath,
    void (ManagerType::*managerFunc)(
        const AuthorizationInfo& authzInfo,
        std::function<void(api::ResultCode, OutputData...)> completionHandler),
    ManagerType* manager,
    EntityType entityType,
    DataActionType dataActionType)
{
    typedef typename nx::utils::tuple_first_element<void, std::tuple<OutputData...>>::type
        ActualOutputDataType;
    typedef AbstractFiniteMsgBodyHttpHandler<
        void,
        typename std::remove_const<typename std::remove_reference<ActualOutputDataType>::type>::type
    > HttpHandlerType;

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
