/**********************************************************
* 8 may 2015
* a.kolesnikov
***********************************************************/

#ifndef CLOUD_DB_PROCESS_H
#define CLOUD_DB_PROCESS_H

//#define USE_QAPPLICATION

#include <atomic>
#include <memory>

#ifdef USE_QAPPLICATION
#include <qtsinglecoreapplication.h>
#include <qtservice.h>
#endif

#include <utils/common/stoppable.h>
#include <utils/db/async_sql_query_executor.h>
#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/utils/std/future.h>

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

class CloudDBProcess
:
#ifdef USE_QAPPLICATION
    public QObject,
    public QtService<QtSingleCoreApplication>,
#endif
    public QnStoppable
{
public:
    CloudDBProcess( int argc, char **argv );

    //!Implementation of QnStoppable::pleaseStop
    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);

    const std::vector<SocketAddress>& httpEndpoints() const;

#ifndef USE_QAPPLICATION
    int exec();
#endif

protected:
#ifdef USE_QAPPLICATION
    virtual int executeApplication() override;
    virtual void start() override;
    virtual void stop() override;

    virtual bool eventFilter(QObject* watched, QEvent* event) override;
#endif

private:
    int m_argc;
    char** m_argv;
    std::atomic<bool> m_terminated;
    int m_timerID;
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> m_startedEventHandler;
    std::vector<SocketAddress> m_httpEndpoints;
#ifndef USE_QAPPLICATION
    nx::utils::promise<void> m_processTerminationEvent;
#endif

    //following pointers are here for debugging convenience
    conf::Settings* m_settings;
    nx::db::AsyncSqlQueryExecutor* m_dbManager;
    nx::utils::TimerManager* m_timerManager;
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

    void initializeLogging( const conf::Settings& settings );
    void registerApiHandlers(
        nx_http::MessageDispatcher* const msgDispatcher,
        const AuthorizationManager& authorizationManager,
        AccountManager* const accountManager,
        SystemManager* const systemManager,
        AuthenticationProvider* const authProvider,
        EventManager* const eventManager);
    bool initializeDB( nx::db::AsyncSqlQueryExecutor* const dbManager );
    bool configureDB( nx::db::AsyncSqlQueryExecutor* const dbManager );
    bool updateDB( nx::db::AsyncSqlQueryExecutor* const dbManager );

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
};

}   //cdb
}   //nx

#endif  //HOLE_PUNCHER_SERVICE_H
