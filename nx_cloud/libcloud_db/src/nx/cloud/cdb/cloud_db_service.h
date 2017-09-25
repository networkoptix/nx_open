#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/http/abstract_msg_body_source.h>
#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/utils/service.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/stoppable.h>

#include <nx/cloud/cdb/api/result_code.h>
#include <nx/utils/stree/resourcecontainer.h>
#include <nx/utils/db/async_sql_query_executor.h>

#include "access_control/auth_types.h"
#include "managers/managers_types.h"
#include "settings.h"


class QnCommandLineParser;

namespace nx_http { class MessageDispatcher; }

namespace nx {

namespace db { class AsyncSqlQueryExecutor; }
namespace utils { class TimerManager; }

namespace cdb {

class AbstractEmailManager;
class StreeManager;
class TemporaryAccountPasswordManager;
class AccountManager;
class EventManager;
class SystemManager;
class SystemHealthInfoProvider;
class AuthenticationManager;
class AuthorizationManager;
class AuthenticationProvider;
class MaintenanceManager;
class CloudModuleUrlProvider;

namespace conf { class Settings; }
namespace ec2 { class ConnectionManager; }

class CloudDbService:
    public nx::utils::Service
{
    using base_type = nx::utils::Service;

public:
    CloudDbService(int argc, char **argv);

    const std::vector<SocketAddress>& httpEndpoints() const;

protected:
    virtual std::unique_ptr<utils::AbstractServiceSettings> createSettings() override;
    virtual int serviceMain(const utils::AbstractServiceSettings& settings) override;

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

    std::vector<SocketAddress> m_httpEndpoints;

    //following pointers are here for debugging convenience
    const conf::Settings* m_settings;
    nx_http::MessageDispatcher* m_httpMessageDispatcher;
    AuthenticationManager* m_authenticationManager;
    AuthorizationManager* m_authorizationManager;

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
