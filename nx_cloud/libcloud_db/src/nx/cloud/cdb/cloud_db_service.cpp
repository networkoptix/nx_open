#include "cloud_db_service.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <list>
#include <thread>
#include <type_traits>

#include <QtCore/QDir>

#include <nx/network/http/auth_restriction_list.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/socket_global.h>
#include <nx/utils/db/db_structure_updater.h>
#include <nx/utils/log/log.h>
#include <nx/utils/platform/current_process.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/system_error.h>
#include <nx/utils/type_utils.h>

#include <nx/cloud/cdb/client/cdb_request_path.h>
#include <nx/cloud/cdb/api/ec2_request_paths.h>

#include "access_control/authentication_manager.h"
#include "controller.h"
#include "http_handlers/get_cloud_modules_xml.h"
#include "http_handlers/ping.h"
#include "libcloud_db_app_info.h"

static int registerQtResources()
{
    Q_INIT_RESOURCE(libcloud_db);
    return 0;
}

namespace nx {
namespace cdb {

CloudDbService::CloudDbService(int argc, char **argv):
    base_type(argc, argv, QnLibCloudDbAppInfo::applicationDisplayName()),
    m_settings(nullptr),
    m_httpMessageDispatcher(nullptr),
    m_authenticationManager(nullptr),
    m_authorizationManager(nullptr)
{
    //if call Q_INIT_RESOURCE directly, linker will search for nx::cdb::libcloud_db and fail...
    registerQtResources();
}

const std::vector<SocketAddress>& CloudDbService::httpEndpoints() const
{
    return m_httpEndpoints;
}

std::unique_ptr<utils::AbstractServiceSettings> CloudDbService::createSettings()
{
    return std::make_unique<conf::Settings>();
}

int CloudDbService::serviceMain(const utils::AbstractServiceSettings& abstractSettings)
{
    const conf::Settings& settings = static_cast<const conf::Settings&>(abstractSettings);

    auto logSettings = settings.vmsSynchronizationLogging();
    logSettings.updateDirectoryIfEmpty(settings.dataDir());
    nx::utils::log::initialize(
        logSettings, QnLibCloudDbAppInfo::applicationDisplayName(), QString(),
        "sync_log", nx::utils::log::addLogger({QnLog::EC2_TRAN_LOG}));

    const auto& httpAddrToListenList = settings.endpointsToListen();
    m_settings = &settings;
    if( httpAddrToListenList.empty() )
    {
        NX_LOG( "No HTTP address to listen", cl_logALWAYS );
        return 1;
    }

    Controller controller(settings);

    nx_http::MessageDispatcher httpMessageDispatcher;
    m_httpMessageDispatcher = &httpMessageDispatcher;

    //TODO #ak move following to stree xml
    nx_http::AuthMethodRestrictionList authRestrictionList;
    authRestrictionList.allow(kDeprecatedCloudModuleXmlPath, nx_http::AuthMethod::noAuth);
    authRestrictionList.allow(kDiscoveryCloudModuleXmlPath, nx_http::AuthMethod::noAuth);
    authRestrictionList.allow(http_handler::Ping::kHandlerPath, nx_http::AuthMethod::noAuth);
    authRestrictionList.allow(kAccountRegisterPath, nx_http::AuthMethod::noAuth);
    authRestrictionList.allow(kAccountActivatePath, nx_http::AuthMethod::noAuth);
    authRestrictionList.allow(kAccountReactivatePath, nx_http::AuthMethod::noAuth);

    std::vector<AbstractAuthenticationDataProvider*> authDataProviders;
    authDataProviders.push_back(&controller.accountManager());
    authDataProviders.push_back(&controller.systemManager());
    AuthenticationManager authenticationManager(
        std::move(authDataProviders),
        authRestrictionList,
        controller.streeManager());
    m_authenticationManager = &authenticationManager;

    AuthorizationManager authorizationManager(
        controller.streeManager(),
        controller.accountManager(),
        controller.systemManager());
    m_authorizationManager = &authorizationManager;

    CloudModuleUrlProvider cloudModuleUrlProviderDeprecated(
        settings.moduleFinder().cloudModulesXmlTemplatePath);
    CloudModuleUrlProvider cloudModuleUrlProvider(
        settings.moduleFinder().newCloudModulesXmlTemplatePath);

    //registering HTTP handlers
    registerApiHandlers(
        &httpMessageDispatcher,
        authorizationManager,
        &controller.accountManager(),
        &controller.systemManager(),
        &controller.systemHealthInfoProvider(),
        &controller.authProvider(),
        &controller.eventManager(),
        &controller.ec2SyncronizationEngine().connectionManager(),
        &controller.maintenanceManager(),
        cloudModuleUrlProviderDeprecated,
        cloudModuleUrlProvider);
    //TODO #ak remove eventManager.registerHttpHandlers and register in registerApiHandlers
    controller.eventManager().registerHttpHandlers(
        authorizationManager,
        &httpMessageDispatcher);

    nx::network::server::MultiAddressServer<nx_http::HttpStreamSocketServer> multiAddressHttpServer(
        &authenticationManager,
        &httpMessageDispatcher,
        false,  //TODO #ak enable ssl when it works properly
        nx::network::NatTraversalSupport::disabled );

    if (m_settings->http().connectionInactivityPeriod > std::chrono::milliseconds::zero())
    {
        multiAddressHttpServer.forEachListener(
            [&](nx_http::HttpStreamSocketServer* server)
            {
                server->setConnectionInactivityTimeout(
                    m_settings->http().connectionInactivityPeriod);
            });
    }

    if (!multiAddressHttpServer.bind(httpAddrToListenList))
        return 3;

    // process privilege reduction
    nx::utils::CurrentProcess::changeUser(settings.changeUser());

    if (!multiAddressHttpServer.listen(settings.http().tcpBacklogSize))
        return 5;
    m_httpEndpoints = multiAddressHttpServer.endpoints();
    NX_LOGX(lm("Listening on %1").container(m_httpEndpoints), cl_logINFO);

    NX_LOG(lm("%1 has been started")
        .arg(QnLibCloudDbAppInfo::applicationDisplayName()), cl_logALWAYS);

    const auto result = runMainLoop();

    NX_LOGX(lm("Stopping..."), cl_logALWAYS);

    // First of all, cancelling accepting new requests.
    multiAddressHttpServer.forEachListener(
        [](nx_http::HttpStreamSocketServer* listener) { listener->pleaseStopSync(); });

    NX_LOGX(lm("Http server stopped"), cl_logDEBUG1);

    return result;
}

void CloudDbService::registerApiHandlers(
    nx_http::MessageDispatcher* const msgDispatcher,
    const AuthorizationManager& authorizationManager,
    AccountManager* const accountManager,
    SystemManager* const systemManager,
    SystemHealthInfoProvider* const systemHealthInfoProvider,
    AuthenticationProvider* const authProvider,
    EventManager* const /*eventManager*/,
    ec2::ConnectionManager* const ec2ConnectionManager,
    MaintenanceManager* const maintenanceManager,
    const CloudModuleUrlProvider& cloudModuleUrlProviderDeprecated,
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
        nx_http::Method::get,
        kEstablishEc2P2pTransactionConnectionPath,
        &ec2::ConnectionManager::createWebsocketTransactionConnection,
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

    registerHttpHandler(
        kMaintenanceGetStatistics,
        &MaintenanceManager::getStatistics, maintenanceManager,
        EntityType::maintenance, DataActionType::fetch);

    //---------------------------------------------------------------------------------------------
    using namespace std::placeholders;

    msgDispatcher->registerRequestProcessor<http_handler::GetCloudModulesXml>(
        kDeprecatedCloudModuleXmlPath,
        [&authorizationManager, &cloudModuleUrlProviderDeprecated]()
            -> std::unique_ptr<http_handler::GetCloudModulesXml>
        {
            return std::make_unique<http_handler::GetCloudModulesXml>(
                std::bind(&CloudModuleUrlProvider::getCloudModulesXml, cloudModuleUrlProviderDeprecated, _1));
        });

    msgDispatcher->registerRequestProcessor<http_handler::GetCloudModulesXml>(
        kDiscoveryCloudModuleXmlPath,
        [&authorizationManager, &cloudModuleUrlProvider]()
            -> std::unique_ptr<http_handler::GetCloudModulesXml>
        {
            return std::make_unique<http_handler::GetCloudModulesXml>(
                std::bind(&CloudModuleUrlProvider::getCloudModulesXml, cloudModuleUrlProvider, _1));
        });
}

template<typename ManagerType, typename InputData, typename... OutputData>
void CloudDbService::registerHttpHandler(
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
void CloudDbService::registerHttpHandler(
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
void CloudDbService::registerHttpHandler(
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

template<typename ManagerType>
void CloudDbService::registerHttpHandler(
    nx_http::Method::ValueType method,
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
        },
        method);
}

} // namespace cdb
} // namespace nx
