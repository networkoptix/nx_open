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

#include <nx/network/auth_restriction_list.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/socket_global.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/log/log.h>
#include <nx/utils/type_utils.h>

#include <api/global_settings.h>
#include <common/common_module.h>
#include <platform/process/current_process.h>
#include <utils/common/app_info.h>
#include <utils/common/guard.h>
#include <utils/common/systemerror.h>
#include <utils/db/db_structure_updater.h>

#include <cloud_db_client/src/cdb_request_path.h>
#include <cdb/ec2_request_paths.h>

#include "access_control/authentication_manager.h"
#include "dao/rdb/db_instance_controller.h"
#include "ec2/synchronization_engine.h"
#include "http_handlers/get_cloud_modules_xml.h"
#include "http_handlers/ping.h"
#include "libcloud_db_app_info.h"
#include "managers/account_manager.h"
#include "managers/auth_provider.h"
#include "managers/cloud_module_url_provider.h"
#include "managers/email_manager.h"
#include "managers/event_manager.h"
#include "managers/maintenance_manager.h"
#include "managers/system_health_info_provider.h"
#include "managers/system_manager.h"
#include "managers/temporary_account_password_manager.h"
#include "stree/stree_manager.h"


static int registerQtResources()
{
    Q_INIT_RESOURCE(libcloud_db);
    return 0;
}

namespace nx {
namespace cdb {

CloudDBProcess::CloudDBProcess(int argc, char **argv):
    m_argc(argc),
    m_argv(argv),
    m_terminated(false),
    m_settings(nullptr),
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
    //if call Q_INIT_RESOURCE directly, linker will search for nx::cdb::libcloud_db and fail...
    registerQtResources();
}

void CloudDBProcess::pleaseStop()
{
    m_processTerminationEvent.set_value();
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

static const QnUuid kCdbGuid("{674bafd7-4eec-4bba-84aa-a1baea7fc6db}");

int CloudDBProcess::exec()
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
        settings.load( m_argc, (const char**) m_argv );
        if( settings.showHelp() )
        {
            settings.printCmdLineArgsHelpToCout();
            return 0;
        }

        nx::utils::log::initialize(
            settings.logging(), settings.dataDir(),
            QnLibCloudDbAppInfo::applicationDisplayName(), "log_file", QnLog::MAIN_LOG_ID);

        nx::utils::log::initialize(
            settings.vmsSynchronizationLogging(), settings.dataDir(),
            QnLibCloudDbAppInfo::applicationDisplayName(), "sync_log", QnLog::EC2_TRAN_LOG);

        const auto& httpAddrToListenList = settings.endpointsToListen();
        m_settings = &settings;
        if( httpAddrToListenList.empty() )
        {
            NX_LOG( "No HTTP address to listen", cl_logALWAYS );
            return 1;
        }

        dao::rdb::DbInstanceController dbInstanceController(
            settings.dbConnectionOptions());
        if (!dbInstanceController.initialize())
        {
            NX_LOG( lit("Failed to initialize DB connection"), cl_logALWAYS );
            return 2;
        }

        nx::utils::TimerManager timerManager;
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

        TemporaryAccountPasswordManager tempPasswordManager(
            settings,
            dbInstanceController.queryExecutor().get());
        m_tempPasswordManager = &tempPasswordManager;

        AccountManager accountManager(
            settings,
            streeManager,
            &tempPasswordManager,
            dbInstanceController.queryExecutor().get(),
            emailManager.get());
        m_accountManager = &accountManager;

        EventManager eventManager(settings);
        m_eventManager = &eventManager;

        ec2::SyncronizationEngine ec2SyncronizationEngine(
            kCdbGuid,
            settings.p2pDb(),
            dbInstanceController.queryExecutor().get());

        SystemHealthInfoProvider systemHealthInfoProvider(
            &ec2SyncronizationEngine.connectionManager(),
            dbInstanceController.queryExecutor().get());

        SystemManager systemManager(
            settings,
            &timerManager,
            &accountManager,
            systemHealthInfoProvider,
            dbInstanceController.queryExecutor().get(),
            emailManager.get(),
            &ec2SyncronizationEngine);
        m_systemManager = &systemManager;

        ec2SyncronizationEngine.subscribeToSystemDeletedNotification(
            systemManager.systemMarkedAsDeletedSubscription());

        //TODO #ak move following to stree xml
        QnAuthMethodRestrictionList authRestrictionList;
        authRestrictionList.allow(http_handler::GetCloudModulesXml::kHandlerPath, AuthMethod::noAuth);
        authRestrictionList.allow(http_handler::Ping::kHandlerPath, AuthMethod::noAuth);
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
            systemManager,
            tempPasswordManager);
        m_authProvider = &authProvider;

        MaintenanceManager maintenanceManager(
            kCdbGuid,
            &ec2SyncronizationEngine);

        CloudModuleUrlProvider cloudModuleUrlProvider(
            settings.moduleFinder().cloudModulesXmlTemplatePath);

        //registering HTTP handlers
        registerApiHandlers(
            &httpMessageDispatcher,
            authorizationManager,
            &accountManager,
            &systemManager,
            &systemHealthInfoProvider,
            &authProvider,
            &eventManager,
            &ec2SyncronizationEngine.connectionManager(),
            &maintenanceManager,
            cloudModuleUrlProvider);
        //TODO #ak remove eventManager.registerHttpHandlers and register in registerApiHandlers
        eventManager.registerHttpHandlers(
            authorizationManager,
            &httpMessageDispatcher);

        MultiAddressServer<nx_http::HttpStreamSocketServer> multiAddressHttpServer(
            &authenticationManager,
            &httpMessageDispatcher,
            false,  //TODO #ak enable ssl when it works properly
            nx::network::NatTraversalSupport::disabled );

        if (m_settings->auth().connectionInactivityPeriod.count())
        {
            multiAddressHttpServer.forEachListener(
                [&](nx_http::HttpStreamSocketServer* server)
                {
                    server->setConnectionInactivityTimeout(
                        m_settings->auth().connectionInactivityPeriod);
                });
        }

        if (!multiAddressHttpServer.bind(httpAddrToListenList))
            return 3;

        // process privilege reduction
        CurrentProcess::changeUser(settings.changeUser());

        if (!multiAddressHttpServer.listen())
            return 5;
        m_httpEndpoints = multiAddressHttpServer.endpoints();

        if (m_terminated)
            return 0;

        NX_LOG(lit("%1 has been started")
                .arg(QnLibCloudDbAppInfo::applicationDisplayName()),
               cl_logALWAYS);

        processStartResult = true;
        triggerOnStartedEventHandlerGuard.fire();

        // This is actually the main loop.
        m_processTerminationEvent.get_future().wait();

        // First of all, cancelling accepting new requests.
        multiAddressHttpServer.forEachListener(
            [](nx_http::HttpStreamSocketServer* listener) { listener->pleaseStopSync(); });

        ec2SyncronizationEngine.unsubscribeFromSystemDeletedNotification(
            systemManager.systemMarkedAsDeletedSubscription());

        return 0;
    }
    catch (const std::exception& e)
    {
        NX_LOG(lit("Failed to start application. %1").arg(e.what()), cl_logALWAYS);
        return 3;
    }
}

void CloudDBProcess::registerApiHandlers(
    nx_http::MessageDispatcher* const msgDispatcher,
    const AuthorizationManager& authorizationManager,
    AccountManager* const accountManager,
    SystemManager* const systemManager,
    SystemHealthInfoProvider* const systemHealthInfoProvider,
    AuthenticationProvider* const authProvider,
    EventManager* const /*eventManager*/,
    ec2::ConnectionManager* const ec2ConnectionManager,
    MaintenanceManager* const maintenanceManager,
    const CloudModuleUrlProvider& cloudModuleUrlProvider)
{
    msgDispatcher->registerRequestProcessor<http_handler::Ping>(
        http_handler::Ping::kHandlerPath,
        [&authorizationManager]() -> std::unique_ptr<http_handler::Ping>
        {
            return std::make_unique<http_handler::Ping>(authorizationManager);
        });

    //---------------------------------------------------------------------------------------------
    // AccountManager
    registerHttpHandler(
        kAccountRegisterPath,
        &AccountManager::registerAccount, accountManager,
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

    //---------------------------------------------------------------------------------------------
    // SystemManager
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
        kSystemRenamePath,
        &SystemManager::updateSystem, systemManager,
        EntityType::system, DataActionType::update);

    registerHttpHandler(
        kSystemUpdatePath,
        &SystemManager::updateSystem, systemManager,
        EntityType::system, DataActionType::update);

    registerHttpHandler(
        kSystemRecordUserSessionStartPath,
        &SystemManager::recordUserSessionStart, systemManager,
        EntityType::account, DataActionType::update);
    //< TODO: #ak: current entity:action is not suitable for this request

    registerHttpHandler(
        kSystemHealthHistoryPath,
        &SystemHealthInfoProvider::getSystemHealthHistory, systemHealthInfoProvider,
        EntityType::system, DataActionType::fetch);

    //---------------------------------------------------------------------------------------------
    // AuthenticationProvider
    registerHttpHandler(
        kAuthGetNoncePath,
        &AuthenticationProvider::getCdbNonce, authProvider,
        EntityType::account, DataActionType::fetch);

    registerHttpHandler(
        kAuthGetAuthenticationPath,
        &AuthenticationProvider::getAuthenticationResponse, authProvider,
        EntityType::account, DataActionType::fetch);

    //---------------------------------------------------------------------------------------------
    // ec2::ConnectionManager
    // TODO: #ak remove after 3.0 release.
    registerHttpHandler(
        kDeprecatedEstablishEc2TransactionConnectionPath,
        &ec2::ConnectionManager::createTransactionConnection,
        ec2ConnectionManager);

    registerHttpHandler(
        kEstablishEc2TransactionConnectionPath,
        &ec2::ConnectionManager::createTransactionConnection,
        ec2ConnectionManager);

    registerHttpHandler(
        //api::kPushEc2TransactionPath,
        nx_http::kAnyPath.toStdString().c_str(), //< Dispatcher does not support max prefix by now.
        &ec2::ConnectionManager::pushTransaction,
        ec2ConnectionManager);

    //---------------------------------------------------------------------------------------------
    // MaintenanceManager
    registerHttpHandler(
        kMaintenanceGetVmsConnections,
        &MaintenanceManager::getVmsConnections, maintenanceManager,
        EntityType::maintenance, DataActionType::fetch);

    registerHttpHandler(
        kMaintenanceGetTransactionLog,
        &MaintenanceManager::getTransactionLog, maintenanceManager,
        EntityType::maintenance, DataActionType::fetch);

    //---------------------------------------------------------------------------------------------
    msgDispatcher->registerRequestProcessor<http_handler::GetCloudModulesXml>(
        http_handler::GetCloudModulesXml::kHandlerPath,
        [&authorizationManager, &cloudModuleUrlProvider]()
            -> std::unique_ptr<http_handler::GetCloudModulesXml>
        {
            return std::make_unique<http_handler::GetCloudModulesXml>(
                cloudModuleUrlProvider);
        });
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

template<typename ManagerType>
void CloudDBProcess::registerHttpHandler(
    const char* handlerPath,
    typename CustomHttpHandler<ManagerType>::ManagerFuncType managerFuncPtr,
    ManagerType* manager)
{
    typedef CustomHttpHandler<ManagerType> RequestHandlerType;

    m_httpMessageDispatcher->registerRequestProcessor<RequestHandlerType>(
        handlerPath,
        [managerFuncPtr, manager]() -> std::unique_ptr<RequestHandlerType>
        {
            return std::make_unique<RequestHandlerType>(manager, managerFuncPtr);
        });
}

} // namespace cdb
} // namespace nx
