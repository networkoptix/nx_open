#pragma once

#include <vector>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/socket_common.h>

#include <nx/cloud/db/api/result_code.h>

#include "http_handlers/get_cloud_modules_xml.h"
#include "managers/managers_types.h"
#include "stree/cdb_ns.h"
#include "statistics/provider.h"

namespace nx::clusterdb::engine { class SyncronizationEngine; }

namespace nx::cloud::db {

class AccountManager;
class AuthenticationProvider;
class AuthorizationInfo;
class CloudModuleUrlProvider;
class SecurityManager;
class Controller;
class EventManager;
class MaintenanceManager;
class SystemManager;
class AbstractSystemHealthInfoProvider;

namespace conf { class Settings; }

class HttpView
{
public:
    using HttpServer =
        nx::network::server::MultiAddressServer<nx::network::http::HttpStreamSocketServer>;

    HttpView(
        const conf::Settings& settings,
        Controller* controller);
    ~HttpView();

    void bind();
    void listen();

    std::vector<network::SocketAddress> endpoints() const;

    const HttpServer& httpServer() const;

    void registerStatisticsApiHandlers(statistics::Provider* statisticsProvider);

private:
    const conf::Settings& m_settings;
    Controller* m_controller;
    nx::network::http::server::rest::MessageDispatcher m_httpMessageDispatcher;
    HttpServer m_multiAddressHttpServer;

    void registerApiHandlers(
        const SecurityManager& securityManager,
        AccountManager* const accountManager,
        SystemManager* const systemManager,
        AbstractSystemHealthInfoProvider* const systemHealthInfoProvider,
        AuthenticationProvider* const authProvider,
        EventManager* const eventManager,
        clusterdb::engine::SyncronizationEngine* const ec2SyncronizationEngine,
        MaintenanceManager* const maintenanceManager,
        const CloudModuleUrlProvider& cloudModuleUrlProviderDeprecated,
        const CloudModuleUrlProvider& cloudModuleUrlProvider);

    /** input & output */
    template<typename ManagerType, typename InputData, typename... OutputData>
    void registerHttpHandler(
        const char* handlerPath,
        void (ManagerType::*managerFunc)(
            const AuthorizationInfo& authzInfo,
            InputData inputData,
            std::function<void(api::ResultCode, OutputData...)> completionHandler),
        ManagerType* manager,
        EntityType entityType,
        DataActionType dataActionType);

    /** no input, output */
    template<typename ManagerType, typename... OutputData>
    void registerHttpHandler(
        const char* handlerPath,
        void (ManagerType::*managerFunc)(
            const AuthorizationInfo& authzInfo,
            std::function<void(api::ResultCode, OutputData...)> completionHandler),
        ManagerType* manager,
        EntityType entityType,
        DataActionType dataActionType);

    /**
     * @param handler is
     * void(const AuthorizationInfo& authzInfo,
     *     const std::vector<nx::network::http::StringType>& restPathParams,
     *     InputData inputData,
     *     std::function<void(api::ResultCode)> completionHandler);
     */
    template<typename InputData, typename HandlerType>
    void registerWriteOnlyRestHandler(
        nx::network::http::Method::ValueType method,
        const char* handlerPath,
        EntityType entityType,
        DataActionType dataActionType,
        HandlerType handler);
};

} // namespace nx::cloud::db
