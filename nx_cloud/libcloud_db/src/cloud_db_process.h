/**********************************************************
* 8 may 2015
* a.kolesnikov
***********************************************************/

#ifndef CLOUD_DB_PROCESS_H
#define CLOUD_DB_PROCESS_H

#include <atomic>
#include <functional>
#include <memory>

#include <utils/common/stoppable.h>
#include <utils/db/async_sql_query_executor.h>
#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/http/abstract_msg_body_source.h>
#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/utils/std/future.h>
#include <plugins/videodecoder/stree/resourcecontainer.h>

#include <cdb/result_code.h>

#include "access_control/auth_types.h"
#include "managers/managers_types.h"
#include "settings.h"


class QnCommandLineParser;

namespace nx_http
{
    class MessageDispatcher;
}

namespace nx {

namespace db {
class AsyncSqlQueryExecutor;
}   //db
namespace utils {
class TimerManager;
}   //utils

namespace cdb {

namespace conf {
class Settings;
}   //conf
class AbstractEmailManager;
class StreeManager;
class TemporaryAccountPasswordManager;
class AccountManager;
class EventManager;
class SystemManager;
class AuthenticationManager;
class AuthorizationManager;
class AuthenticationProvider;
namespace ec2 {
class ConnectionManager;
}   // namespace ec2
class MaintenanceManager;

class CloudDBProcess:
    public QnStoppable
{
public:
    CloudDBProcess( int argc, char **argv );

    //!Implementation of QnStoppable::pleaseStop
    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);

    const std::vector<SocketAddress>& httpEndpoints() const;

    int exec();

private:
    template<typename ManagerType>
    class CustomHttpHandler:
        public nx_http::AbstractHttpRequestHandler
    {
    public:
        typedef void (ManagerType::*ManagerFuncType)(
            nx_http::HttpServerConnection* const connection,
            stree::ResourceContainer authInfo,
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
            stree::ResourceContainer authInfo,
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

    int m_argc;
    char** m_argv;
    std::atomic<bool> m_terminated;
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> m_startedEventHandler;
    std::vector<SocketAddress> m_httpEndpoints;
    nx::utils::promise<void> m_processTerminationEvent;

    //following pointers are here for debugging convenience
    conf::Settings* m_settings;
    AbstractEmailManager* m_emailManager;
    StreeManager* m_streeManager;
    nx_http::MessageDispatcher* m_httpMessageDispatcher;
    TemporaryAccountPasswordManager* m_tempPasswordManager;
    AccountManager* m_accountManager;
    EventManager* m_eventManager;
    SystemManager* m_systemManager;
    AuthenticationManager* m_authenticationManager;
    AuthorizationManager* m_authorizationManager;
    AuthenticationProvider* m_authProvider;

    void registerApiHandlers(
        nx_http::MessageDispatcher* const msgDispatcher,
        const AuthorizationManager& authorizationManager,
        AccountManager* const accountManager,
        SystemManager* const systemManager,
        AuthenticationProvider* const authProvider,
        EventManager* const eventManager,
        ec2::ConnectionManager* const ec2ConnectionManager,
        MaintenanceManager* const maintenanceManager);

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
};

} // namespace cdb
} // namespace nx

#endif  //HOLE_PUNCHER_SERVICE_H
