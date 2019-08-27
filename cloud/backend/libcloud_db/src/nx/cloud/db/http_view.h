#pragma once

#include <vector>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/maintenance/server.h>
#include <nx/network/socket_common.h>
#include <nx/network/http/server/authentication_dispatcher.h>
#include <nx/network/http/server/http_server_base_authentication_manager.h>
#include <nx/network/http/server/http_server_htdigest_authentication_provider.h>

#include <nx/cloud/db/api/result_code.h>

#include "http_handlers/get_cloud_modules_xml.h"
#include "managers/managers_types.h"
#include "stree/cdb_ns.h"
#include "statistics/provider.h"

namespace nx::clusterdb::engine { class SynchronizationEngine; }

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
    class HttpServer;

    HttpView(
        const conf::Settings& settings,
        Controller* controller);
    ~HttpView();

    void bind();
    void listen();

    std::vector<network::SocketAddress> endpoints() const;

    const HttpServer& httpServer() const;

    void registerStatisticsApiHandlers(statistics::Provider* statisticsProvider);

public:
    class HttpServer:
        public network::server::MultiAddressServer<nx::network::http::HttpStreamSocketServer>,
        public network::http::server::AbstractHttpStatisticsProvider
    {
        using base_type =
            network::server::MultiAddressServer<nx::network::http::HttpStreamSocketServer>;

        friend void HttpView::listen();

    public:
        template<typename... Args>
        HttpServer(Args... args):
            base_type(std::forward<decltype(args)>(args)...)
        {
        }

        virtual network::http::server::HttpStatistics httpStatistics() const override;

    private:
        void initializeHttpStatisticsProvider();

    private:
        std::unique_ptr<
            network::http::server::AggregateHttpStatisticsProvider> m_httpStatsProvider;
    };

private:
    /** Provides htdigest authentication for maintenance server*/
    struct HtdigestAuthenticator
    {
        nx::network::http::server::HtdigestAuthenticationProvider provider;
        nx::network::http::server::BaseAuthenticationManager manager;

        HtdigestAuthenticator(const std::string& htdigestPath):
            provider(htdigestPath),
            manager(&provider)
        {
        }
    };

    const conf::Settings& m_settings;
    Controller* m_controller;
    nx::network::http::server::rest::MessageDispatcher m_httpMessageDispatcher;
    nx::network::http::server::AuthenticationDispatcher m_authenticationDispatcher;
    HttpServer m_multiAddressHttpServer;
    std::unique_ptr<HtdigestAuthenticator> m_htdigestAuthenticator;
    network::maintenance::Server m_maintenanceServer;

    void registerAuthenticators();

    void registerApiHandlers(
        const SecurityManager& securityManager,
        AccountManager* const accountManager,
        SystemManager* const systemManager,
        AbstractSystemHealthInfoProvider* const systemHealthInfoProvider,
        AuthenticationProvider* const authProvider,
        EventManager* const eventManager,
        clusterdb::engine::SynchronizationEngine* const ec2SynchronizationEngine,
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
            std::function<void(api::Result, OutputData...)> completionHandler),
        ManagerType* manager,
        EntityType entityType,
        DataActionType dataActionType);

    /** no input, output */
    template<typename ManagerType, typename... OutputData>
    void registerHttpHandler(
        const char* handlerPath,
        void (ManagerType::*managerFunc)(
            const AuthorizationInfo& authzInfo,
            std::function<void(api::Result, OutputData...)> completionHandler),
        ManagerType* manager,
        EntityType entityType,
        DataActionType dataActionType);

    template<typename InputData, typename... OutputData,
        typename Func, typename... RestParamFetchers
    >
    void registerRestHttpHandler(
        const char* httpMethod,
        const char* path,
        Func func,
        EntityType entityType,
        DataActionType dataActionType,
        RestParamFetchers...);

    /**
     * @param handler is
     * void(const AuthorizationInfo& authzInfo,
     *     const std::vector<nx::network::http::StringType>& restPathParams,
     *     InputData inputData,
     *     std::function<void(api::Result)> completionHandler);
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
