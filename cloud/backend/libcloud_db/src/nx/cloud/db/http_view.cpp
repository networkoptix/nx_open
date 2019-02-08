#include "http_view.h"

#include <memory>
#include <system_error>

#include <nx/cloud/db/client/cdb_request_path.h>
#include <nx/cloud/db/api/ec2_request_paths.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "controller.h"
#include "http_handlers/ping.h"
#include "settings.h"

namespace nx::cloud::db {

HttpView::HttpView(
    const conf::Settings& settings,
    Controller* controller)
    :
    m_settings(settings),
    m_controller(controller),
    m_multiAddressHttpServer(
        &controller->securityManager().authenticator(),
        &m_httpMessageDispatcher,
        false,  //< TODO: #ak enable ssl when it works properly.
        nx::network::NatTraversalSupport::disabled)
{
    registerApiHandlers(
        controller->securityManager(),
        &controller->accountManager(),
        &controller->systemManager(),
        &controller->systemHealthInfoProvider(),
        &controller->authProvider(),
        &controller->eventManager(),
        &controller->ec2SyncronizationEngine(),
        &controller->maintenanceManager(),
        controller->cloudModuleUrlProviderDeprecated(),
        controller->cloudModuleUrlProvider());

    // TODO: #ak Remove eventManager.registerHttpHandlers and register in registerApiHandlers.
    controller->eventManager().registerHttpHandlers(
        controller->securityManager(),
        &m_httpMessageDispatcher);

    if (settings.http().connectionInactivityPeriod > std::chrono::milliseconds::zero())
    {
        m_multiAddressHttpServer.forEachListener(
            [&settings](nx::network::http::HttpStreamSocketServer* server)
            {
                server->setConnectionInactivityTimeout(
                    settings.http().connectionInactivityPeriod);
            });
    }

    m_maintenanceServer.registerRequestHandlers(
        kApiPrefix,
        &m_httpMessageDispatcher);
}

HttpView::~HttpView()
{
    // First of all, cancelling accepting new requests.
    m_multiAddressHttpServer.forEachListener(
        [](nx::network::http::HttpStreamSocketServer* listener) { listener->pleaseStopSync(); });

    NX_INFO(this, lm("Http server stopped"));
}

void HttpView::bind()
{
    const auto& httpAddrToListenList = m_settings.endpointsToListen();
    if (httpAddrToListenList.empty())
    {
        NX_ALWAYS(this, "No HTTP address to listen");
        throw std::runtime_error("No HTTP address to listen");
    }

    if (!m_multiAddressHttpServer.bind(httpAddrToListenList))
        throw std::system_error(SystemError::getLastOSErrorCode(), std::system_category());
}

void HttpView::listen()
{
    if (!m_multiAddressHttpServer.listen(m_settings.http().tcpBacklogSize))
        throw std::system_error(SystemError::getLastOSErrorCode(), std::system_category());
}

std::vector<network::SocketAddress> HttpView::endpoints() const
{
    return m_multiAddressHttpServer.endpoints();
}

const HttpView::HttpServer& HttpView::httpServer() const
{
    return m_multiAddressHttpServer;
}

void HttpView::registerStatisticsApiHandlers(
    statistics::Provider* statisticsProvider)
{
    network::http::registerFusionRequestHandler(
        &m_httpMessageDispatcher,
        kStatisticsMetricsPath,
        std::bind(&statistics::Provider::statistics, statisticsProvider),
        network::http::Method::get);
}

void HttpView::registerApiHandlers(
    const SecurityManager& securityManager,
    AccountManager* const accountManager,
    SystemManager* const systemManager,
    AbstractSystemHealthInfoProvider* const systemHealthInfoProvider,
    AuthenticationProvider* const authProvider,
    EventManager* const /*eventManager*/,
    clusterdb::engine::SyncronizationEngine* const ec2SyncronizationEngine,
    MaintenanceManager* const maintenanceManager,
    const CloudModuleUrlProvider& cloudModuleUrlProviderDeprecated,
    const CloudModuleUrlProvider& cloudModuleUrlProvider)
{
    m_httpMessageDispatcher.registerRequestProcessor<http_handler::Ping>(
        http_handler::Ping::kHandlerPath,
        [&securityManager]() -> std::unique_ptr<http_handler::Ping>
        {
            return std::make_unique<http_handler::Ping>(securityManager);
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
        EntityType::system, DataActionType::delete_);

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
        &AbstractSystemHealthInfoProvider::getSystemHealthHistory, systemHealthInfoProvider,
        EntityType::system, DataActionType::fetch);

    //---------------------------------------------------------------------------------------------
    // SystemMergeManager
    registerWriteOnlyRestHandler<data::SystemId>(
        nx::network::http::Method::post,
        kSystemsMergedToASpecificSystem,
        EntityType::system, DataActionType::insert,
        [this](
            const AuthorizationInfo& authzInfo,
            const network::http::RequestPathParams& restPathParams,
            data::SystemId inputData,
            std::function<void(api::Result)> completionHandler)
        {
            m_controller->systemMergeManager().startMergingSystems(
                authzInfo,
                restPathParams.getByName(kSystemIdParam),
                std::move(inputData.systemId),
                std::move(completionHandler));
        });

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
    ec2SyncronizationEngine->registerHttpApi(
        kEc2TransactionConnectionPathPrefix, &m_httpMessageDispatcher);

    ec2SyncronizationEngine->registerHttpApi(
        kDeprecatedEc2TransactionConnectionPathPrefix, &m_httpMessageDispatcher);

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

    m_httpMessageDispatcher.registerRequestProcessor<http_handler::GetCloudModulesXml>(
        kDeprecatedCloudModuleXmlPath,
        [&cloudModuleUrlProviderDeprecated]()
            -> std::unique_ptr<http_handler::GetCloudModulesXml>
        {
            return std::make_unique<http_handler::GetCloudModulesXml>(
                std::bind(&CloudModuleUrlProvider::getCloudModulesXml, cloudModuleUrlProviderDeprecated, _1));
        });

    m_httpMessageDispatcher.registerRequestProcessor<http_handler::GetCloudModulesXml>(
        kAnotherDeprecatedCloudModuleXmlPath,
        [&cloudModuleUrlProviderDeprecated]()
            -> std::unique_ptr<http_handler::GetCloudModulesXml>
        {
            return std::make_unique<http_handler::GetCloudModulesXml>(
                std::bind(&CloudModuleUrlProvider::getCloudModulesXml, cloudModuleUrlProviderDeprecated, _1));
        });

    m_httpMessageDispatcher.registerRequestProcessor<http_handler::GetCloudModulesXml>(
        kDiscoveryCloudModuleXmlPath,
        [&cloudModuleUrlProvider]()
            -> std::unique_ptr<http_handler::GetCloudModulesXml>
        {
            return std::make_unique<http_handler::GetCloudModulesXml>(
                std::bind(&CloudModuleUrlProvider::getCloudModulesXml, cloudModuleUrlProvider, _1));
        });
}

template<typename ManagerType, typename InputData, typename... OutputData>
void HttpView::registerHttpHandler(
    const char* handlerPath,
    void (ManagerType::*managerFunc)(
        const AuthorizationInfo& authzInfo,
        InputData inputData,
        std::function<void(api::Result, OutputData...)> completionHandler),
    ManagerType* manager,
    EntityType entityType,
    DataActionType dataActionType)
{
    typedef FiniteMsgBodyHttpHandler<
        typename std::remove_const<typename std::remove_reference<InputData>::type>::type,
        OutputData...
    > HttpHandlerType;

    m_httpMessageDispatcher.registerRequestProcessor<HttpHandlerType>(
        handlerPath,
        [this, managerFunc, manager, entityType, dataActionType]()
            -> std::unique_ptr<HttpHandlerType>
        {
            using namespace std::placeholders;
            return std::make_unique<HttpHandlerType>(
                entityType,
                dataActionType,
                m_controller->securityManager(),
                std::bind(managerFunc, manager, _1, _2, _3));
        });
}

template<typename ManagerType, typename... OutputData>
void HttpView::registerHttpHandler(
    const char* handlerPath,
    void (ManagerType::*managerFunc)(
        const AuthorizationInfo& authzInfo,
        std::function<void(api::Result, OutputData...)> completionHandler),
    ManagerType* manager,
    EntityType entityType,
    DataActionType dataActionType)
{
    using ActualOutputDataType =
        typename nx::utils::tuple_first_element<void, std::tuple<OutputData...>>::type;

    using HttpHandlerType = FiniteMsgBodyHttpHandler<
        void,
        typename std::remove_const<
            typename std::remove_reference<ActualOutputDataType>::type>::type>;

    m_httpMessageDispatcher.registerRequestProcessor<HttpHandlerType>(
        handlerPath,
        [this, managerFunc, manager, entityType, dataActionType]()
            -> std::unique_ptr<HttpHandlerType>
        {
            return std::make_unique<HttpHandlerType>(
                entityType,
                dataActionType,
                m_controller->securityManager(),
                [managerFunc, manager](auto&&... args)
                {
                    (manager->*managerFunc)(std::move(args)...);
                });
        });
}

namespace {

template<typename Input, typename HandlerFunc>
class WriteOnlyRestHandler:
    public AbstractFiniteMsgBodyHttpHandler<Input>
{
    using base_type = AbstractFiniteMsgBodyHttpHandler<Input>;

public:
    //using HandlerFunc =
    //    void(const AuthorizationInfo& authzInfo,
    //        const std::vector<nx::network::http::StringType>& restPathParams,
    //        Input inputData,
    //        std::function<void(api::Result)> completionHandler);

    WriteOnlyRestHandler(
        EntityType entityType,
        DataActionType actionType,
        const SecurityManager& securityManager,
        HandlerFunc func)
        :
        base_type(entityType, actionType, securityManager),
        m_func(std::move(func))
    {
    }

protected:
    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        Input inputData,
        std::function<void(api::Result)> completionHandler) override
    {
        m_func(
            AuthorizationInfo(std::exchange(requestContext.authInfo, {})),
            std::exchange(requestContext.requestPathParams, {}),
            std::move(inputData),
            std::move(completionHandler));
    }

private:
    HandlerFunc m_func;
};

} // namespace

template<typename InputData, typename HandlerType>
void HttpView::registerWriteOnlyRestHandler(
    nx::network::http::Method::ValueType method,
    const char* handlerPath,
    EntityType entityType,
    DataActionType dataActionType,
    HandlerType handler)
{
    using HttpHandlerType = WriteOnlyRestHandler<InputData, HandlerType>;

    m_httpMessageDispatcher.registerRequestProcessor<HttpHandlerType>(
        handlerPath,
        [this, entityType, dataActionType, handler]() -> std::unique_ptr<HttpHandlerType>
        {
            return std::make_unique<HttpHandlerType>(
                entityType,
                dataActionType,
                m_controller->securityManager(),
                handler);
        },
        method);
}

} // namespace nx::cloud::db
