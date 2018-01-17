#include "http_view.h"

#include <memory>
#include <system_error>

#include <nx/cloud/cdb/client/cdb_request_path.h>
#include <nx/cloud/cdb/api/ec2_request_paths.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "controller.h"
#include "http_handlers/ping.h"
#include "settings.h"

namespace nx {
namespace cdb {

HttpView::HttpView(
    const conf::Settings& settings,
    Controller* controller)
    :
    m_settings(settings),
    m_controller(controller),
    m_multiAddressHttpServer(
        &controller->authenticationManager(),
        &m_httpMessageDispatcher,
        false,  //< TODO: #ak enable ssl when it works properly.
        nx::network::NatTraversalSupport::disabled)
{
    registerApiHandlers(
        controller->authorizationManager(),
        &controller->accountManager(),
        &controller->systemManager(),
        &controller->systemHealthInfoProvider(),
        &controller->authProvider(),
        &controller->eventManager(),
        &controller->ec2SyncronizationEngine().connectionManager(),
        &controller->maintenanceManager(),
        controller->cloudModuleUrlProviderDeprecated(),
        controller->cloudModuleUrlProvider());

    // TODO: #ak Remove eventManager.registerHttpHandlers and register in registerApiHandlers.
    controller->eventManager().registerHttpHandlers(
        controller->authorizationManager(),
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
        NX_LOG("No HTTP address to listen", cl_logALWAYS);
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

void HttpView::registerApiHandlers(
    const AuthorizationManager& authorizationManager,
    AccountManager* const accountManager,
    SystemManager* const systemManager,
    AbstractSystemHealthInfoProvider* const systemHealthInfoProvider,
    AuthenticationProvider* const authProvider,
    EventManager* const /*eventManager*/,
    ec2::ConnectionManager* const ec2ConnectionManager,
    MaintenanceManager* const maintenanceManager,
    const CloudModuleUrlProvider& cloudModuleUrlProviderDeprecated,
    const CloudModuleUrlProvider& cloudModuleUrlProvider)
{
    m_httpMessageDispatcher.registerRequestProcessor<http_handler::Ping>(
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
            const std::vector<nx::network::http::StringType>& restPathParams,
            data::SystemId inputData,
            std::function<void(api::ResultCode)> completionHandler)
        {
            m_controller->systemMergeManager().startMergingSystems(
                authzInfo,
                restPathParams[0].toStdString(),
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
        nx::network::http::Method::get,
        kEstablishEc2P2pTransactionConnectionPath,
        &ec2::ConnectionManager::createWebsocketTransactionConnection,
        ec2ConnectionManager);

    registerHttpHandler(
        //api::kPushEc2TransactionPath,
        nx::network::http::kAnyPath.toStdString().c_str(), //< Dispatcher does not support max prefix by now.
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

    m_httpMessageDispatcher.registerRequestProcessor<http_handler::GetCloudModulesXml>(
        kDeprecatedCloudModuleXmlPath,
        [&authorizationManager, &cloudModuleUrlProviderDeprecated]()
            -> std::unique_ptr<http_handler::GetCloudModulesXml>
        {
            return std::make_unique<http_handler::GetCloudModulesXml>(
                std::bind(&CloudModuleUrlProvider::getCloudModulesXml, cloudModuleUrlProviderDeprecated, _1));
        });

    m_httpMessageDispatcher.registerRequestProcessor<http_handler::GetCloudModulesXml>(
        kDiscoveryCloudModuleXmlPath,
        [&authorizationManager, &cloudModuleUrlProvider]()
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
        std::function<void(api::ResultCode, OutputData...)> completionHandler),
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
                m_controller->authorizationManager(),
                std::bind(managerFunc, manager, _1, _2, _3));
        });
}

template<typename ManagerType, typename... OutputData>
void HttpView::registerHttpHandler(
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
    typedef FiniteMsgBodyHttpHandler<
        void,
        typename std::remove_const<typename std::remove_reference<ActualOutputDataType>::type>::type
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
                m_controller->authorizationManager(),
                std::bind(managerFunc, manager, _1, _2));
        });
}

template<typename ManagerType>
void HttpView::registerHttpHandler(
    const char* handlerPath,
    typename CustomHttpHandler<ManagerType>::ManagerFuncType managerFuncPtr,
    ManagerType* manager)
{
    typedef CustomHttpHandler<ManagerType> RequestHandlerType;

    m_httpMessageDispatcher.registerRequestProcessor<RequestHandlerType>(
        handlerPath,
        [managerFuncPtr, manager]() -> std::unique_ptr<RequestHandlerType>
        {
            return std::make_unique<RequestHandlerType>(manager, managerFuncPtr);
        });
}

template<typename ManagerType>
void HttpView::registerHttpHandler(
    nx::network::http::Method::ValueType method,
    const char* handlerPath,
    typename CustomHttpHandler<ManagerType>::ManagerFuncType managerFuncPtr,
    ManagerType* manager)
{
    typedef CustomHttpHandler<ManagerType> RequestHandlerType;

    m_httpMessageDispatcher.registerRequestProcessor<RequestHandlerType>(
        handlerPath,
        [managerFuncPtr, manager]() -> std::unique_ptr<RequestHandlerType>
        {
            return std::make_unique<RequestHandlerType>(manager, managerFuncPtr);
        },
        method);
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
    //        std::function<void(api::ResultCode)> completionHandler);

    WriteOnlyRestHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager,
        HandlerFunc func)
        :
        base_type(entityType, actionType, authorizationManager),
        m_func(std::move(func))
    {
    }

protected:
    virtual void processRequest(
        const AuthorizationInfo& authzInfo,
        Input inputData,
        std::function<void(api::ResultCode)> completionHandler) override
    {
        m_func(
            authzInfo,
            this->requestPathParams(),
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
                m_controller->authorizationManager(),
                handler);
        },
        method);
}

} // namespace cdb
} // namespace nx
