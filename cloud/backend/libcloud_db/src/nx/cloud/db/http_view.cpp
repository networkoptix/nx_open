#include "http_view.h"

#include <memory>
#include <system_error>

#include <nx/cloud/db/client/cdb_request_path.h>
#include <nx/cloud/db/api/ec2_request_paths.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/server/rest/base_request_handler.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "controller.h"
#include "http_handlers/base_http_handler.h"
#include "settings.h"

namespace nx::cloud::db {

void HttpView::HttpServer::initializeHttpStatisticsProvider()
{
    using namespace network::http::server;
    std::vector<const AbstractHttpStatisticsProvider*> providers;
    forEachListener(
        [&providers](const auto& listener)
        {
            providers.emplace_back(listener);
        });

    m_httpStatsProvider = std::make_unique<AggregateHttpStatisticsProvider>(std::move(providers));
}

network::http::server::HttpStatistics HttpView::HttpServer::httpStatistics() const
{
    return m_httpStatsProvider
        ? m_httpStatsProvider->httpStatistics()
        : network::http::server::HttpStatistics();
}

//-------------------------------------------------------------------------------------------------
// HttpView

HttpView::HttpView(
    const conf::Settings& settings,
    Controller* controller)
    :
    m_settings(settings),
    m_controller(controller),
    m_multiAddressHttpServer(
        &m_authenticationDispatcher,
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
        &controller->ec2SynchronizationEngine(),
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

    m_maintenanceServer.registerRequestHandlers(kApiPrefix, &m_httpMessageDispatcher);

    registerAuthenticators();
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
        NX_ERROR(this, "No HTTP address to listen");
        throw std::runtime_error("No HTTP address to listen");
    }

    if (!m_multiAddressHttpServer.bind(httpAddrToListenList))
        throw std::system_error(SystemError::getLastOSErrorCode(), std::system_category());
}

void HttpView::listen()
{
    if (!m_multiAddressHttpServer.listen(m_settings.http().tcpBacklogSize))
        throw std::system_error(SystemError::getLastOSErrorCode(), std::system_category());

    m_multiAddressHttpServer.initializeHttpStatisticsProvider();

    NX_INFO(this, "HTTP server is listening on %1",
        containerString(m_multiAddressHttpServer.endpoints()));
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

void HttpView::registerAuthenticators()
{
    const auto addPathToHtDigestAuthenticator =
        [this](const std::string& regex)
        {
            NX_INFO(this, "Adding api path regex: '%1' to htdigest authenticator", regex);
            m_authenticationDispatcher.add(std::regex(regex), &m_htdigestAuthenticator->manager);
        };

    if (!m_settings.http().maintenanceHtdigestPath.empty())
    {
        NX_INFO(this,
            "htdigest authentication for cloud db maintenance server enabled. File path: %1",
            m_settings.http().maintenanceHtdigestPath);

        m_htdigestAuthenticator = std::make_unique<HtdigestAuthenticator>(
            m_settings.http().maintenanceHtdigestPath);

        addPathToHtDigestAuthenticator(
            network::url::joinPath(m_maintenanceServer.maintenancePath(), "/.*"));

        addPathToHtDigestAuthenticator(network::url::joinPath(kStatisticsPath, "/.*"));
    }

    m_authenticationDispatcher.add(
        std::regex(".*"),
        &m_controller->securityManager().authenticator());
}

void HttpView::registerApiHandlers(
    const SecurityManager& /*securityManager*/,
    AccountManager* const accountManager,
    SystemManager* const systemManager,
    AbstractSystemHealthInfoProvider* const systemHealthInfoProvider,
    AuthenticationProvider* const authProvider,
    EventManager* const /*eventManager*/,
    clusterdb::engine::SynchronizationEngine* const ec2SynchronizationEngine,
    MaintenanceManager* const maintenanceManager,
    const CloudModuleUrlProvider& cloudModuleUrlProviderDeprecated,
    const CloudModuleUrlProvider& cloudModuleUrlProvider)
{
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

    registerRestHttpHandler<api::UserAuthorization, api::CredentialsDescriptor>(
        nx::network::http::Method::post.constData(),
        kAuthResolveUserCredentials,
        [authProvider](auto&&... args)
            { authProvider->resolveUserCredentials(std::forward<decltype(args)>(args)...); },
        EntityType::account, DataActionType::fetch);

    registerRestHttpHandler<api::UserAuthorization, api::SystemAccess>(
        nx::network::http::Method::post.constData(),
        kAuthSystemAccessLevel,
        [authProvider](auto&&... args)
            { authProvider->getSystemAccessLevel(std::forward<decltype(args)>(args)...); },
        EntityType::account, DataActionType::fetch,
        nx::network::http::server::rest::RestArgFetcher<kSystemIdParam>());

    //---------------------------------------------------------------------------------------------
    ec2SynchronizationEngine->registerHttpApi(
        kEc2TransactionConnectionPathPrefix, &m_httpMessageDispatcher);

    ec2SynchronizationEngine->registerHttpApi(
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

    registerHttpHandler(
        kPingPath,
        &MaintenanceManager::processPing, maintenanceManager,
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
    registerRestHttpHandler<
        std::remove_cv_t<std::remove_reference_t<InputData>>,
        std::remove_cv_t<std::remove_reference_t<OutputData>>...
    >(
        nx::network::http::kAnyMethod,
        handlerPath,
        [manager, managerFunc](auto&&... args)
            { (manager->*managerFunc)(std::forward<decltype(args)>(args)...); },
        entityType, dataActionType);
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
    registerRestHttpHandler<
        void,
        std::remove_cv_t<std::remove_reference_t<OutputData>>...
    >(
        nx::network::http::kAnyMethod,
        handlerPath,
        [manager, managerFunc](auto&&... args)
            { (manager->*managerFunc)(std::forward<decltype(args)>(args)...); },
        entityType, dataActionType);
}

template<typename InputData, typename... OutputData,
    typename Func, typename... RestParamFetchers
>
void HttpView::registerRestHttpHandler(
    const char* httpMethod,
    const char* path,
    Func func,
    EntityType entityType,
    DataActionType dataActionType,
    RestParamFetchers...)
{
    using HttpHandler = FiniteMsgBodyRestHandler<InputData, OutputData...>;

    if constexpr (std::is_same_v<InputData, void>)
    {
        // TODO: #ak Move if constexpr inside lambda after switching to msvc2019
        // (when there is no "if constexpr inside lambda inside a template function" bug).
        m_httpMessageDispatcher.registerRequestProcessor<HttpHandler>(
            path,
            [this, func = std::move(func), entityType, dataActionType]() mutable
                -> std::unique_ptr<HttpHandler>
            {
                return std::make_unique<HttpHandler>(
                    entityType,
                    dataActionType,
                    m_controller->securityManager(),
                    [func = std::move(func)](
                        nx::network::http::RequestContext requestContext,
                        auto completionHandler)
                    {
                        func(
                            AuthorizationInfo(std::exchange(requestContext.authInfo, {})),
                            RestParamFetchers()(requestContext)...,
                            std::move(completionHandler));
                    });
            },
            httpMethod);
    }
    else
    {
        m_httpMessageDispatcher.registerRequestProcessor<HttpHandler>(
            path,
            [this, func = std::move(func), entityType, dataActionType]() mutable
                -> std::unique_ptr<HttpHandler>
            {
                return std::make_unique<HttpHandler>(
                    entityType,
                    dataActionType,
                    m_controller->securityManager(),
                    [func = std::move(func)](
                        nx::network::http::RequestContext requestContext,
                        InputData input,
                        auto completionHandler)
                    {
                        func(
                            AuthorizationInfo(std::exchange(requestContext.authInfo, {})),
                            RestParamFetchers()(requestContext)...,
                            std::move(input),
                            std::move(completionHandler));
                    });
            },
            httpMethod);
    }
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
