#pragma once

#include <vector>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/socket_common.h>

#include <nx/cloud/cdb/api/result_code.h>

#include "http_handlers/get_cloud_modules_xml.h"
#include "managers/managers_types.h"

namespace nx {
namespace cdb {

class AccountManager;
class AuthenticationProvider;
class AuthorizationInfo;
class CloudModuleUrlProvider;
class Controller;
class EventManager;
class MaintenanceManager;
class SystemManager;
class SystemHealthInfoProvider;

namespace conf { class Settings; }
namespace ec2{ class ConnectionManager; }

class HttpView
{
public:
    HttpView(
        const conf::Settings& settings,
        Controller* controller);
    ~HttpView();

    void bind();
    void listen();

    std::vector<SocketAddress> endpoints() const;

private:
    template<typename ManagerType>
    class CustomHttpHandler:
        public nx_http::AbstractHttpRequestHandler
    {
    public:
        typedef void (ManagerType::*ManagerFuncType)(
            nx_http::HttpServerConnection* const connection,
            nx::utils::stree::ResourceContainer authInfo,
            nx_http::Request request,
            nx_http::Response* const response,
            nx_http::RequestProcessedHandler completionHandler);

        CustomHttpHandler(
            ManagerType* manager,
            ManagerFuncType managerFuncPtr)
            :
            m_manager(manager),
            m_managerFuncPtr(managerFuncPtr)
        {
        }

    protected:
        virtual void processRequest(
            nx_http::HttpServerConnection* const connection,
            nx::utils::stree::ResourceContainer authInfo,
            nx_http::Request request,
            nx_http::Response* const response,
            nx_http::RequestProcessedHandler completionHandler) override
        {
            (m_manager->*m_managerFuncPtr)(
                connection,
                std::move(authInfo),
                std::move(request),
                response,
                std::move(completionHandler));
        }

    private:
        ManagerType* m_manager;
        ManagerFuncType m_managerFuncPtr;
    };

    const conf::Settings& m_settings;
    Controller* m_controller;
    nx_http::MessageDispatcher m_httpMessageDispatcher;
    nx::network::server::MultiAddressServer<nx_http::HttpStreamSocketServer> m_multiAddressHttpServer;

    void registerApiHandlers(
        nx_http::MessageDispatcher* const msgDispatcher,
        const AuthorizationManager& authorizationManager,
        AccountManager* const accountManager,
        SystemManager* const systemManager,
        SystemHealthInfoProvider* const systemHealthInfoProvider,
        AuthenticationProvider* const authProvider,
        EventManager* const eventManager,
        ec2::ConnectionManager* const ec2ConnectionManager,
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

    template<typename ManagerType>
    void registerHttpHandler(
        const char* handlerPath,
        typename CustomHttpHandler<ManagerType>::ManagerFuncType managerFuncPtr,
        ManagerType* manager);

    template<typename ManagerType>
    void registerHttpHandler(
        nx_http::Method::ValueType method,
        const char* handlerPath,
        typename CustomHttpHandler<ManagerType>::ManagerFuncType managerFuncPtr,
        ManagerType* manager);
};

} // namespace cdb
} // namespace nx
