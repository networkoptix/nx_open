/**********************************************************
* 8 may 2015
* a.kolesnikov
***********************************************************/

#ifndef CLOUD_DB_PROCESS_H
#define CLOUD_DB_PROCESS_H

#include <atomic>
#include <memory>

#include <qtsinglecoreapplication.h>
#include <qtservice.h>

#include <utils/common/stoppable.h>
#include <utils/db/db_manager.h>
#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/http/server/http_stream_socket_server.h>

#include "settings.h"


class QnCommandLineParser;

namespace nx_http
{
    class MessageDispatcher;
}

namespace nx {
namespace cdb {

class AuthorizationManager;
class AccountManager;
class SystemManager;
class AuthenticationProvider;

class CloudDBProcess
:
    public QObject,
    public QtService<QtSingleCoreApplication>,
    public QnStoppable
{
public:
    CloudDBProcess( int argc, char **argv );

    //!Implementation of QnStoppable::pleaseStop
    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);

protected:
    virtual int executeApplication() override;
    virtual void start() override;
    virtual void stop() override;

    virtual bool eventFilter(QObject* watched, QEvent* event) override;

private:
    int m_argc;
    char** m_argv;
    std::atomic<bool> m_terminated;
    int m_timerID;
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> m_startedEventHandler;

    void initializeLogging( const conf::Settings& settings );
    void registerApiHandlers(
        nx_http::MessageDispatcher* const msgDispatcher,
        const AuthorizationManager& authorizationManager,
        AccountManager* const accountManager,
        SystemManager* const systemManager,
        AuthenticationProvider* const authProvider);
    bool initializeDB( nx::db::DBManager* const dbManager );
    bool configureDB( nx::db::DBManager* const dbManager );
    bool updateDB( nx::db::DBManager* const dbManager );
};

}   //cdb
}   //nx

#endif  //HOLE_PUNCHER_SERVICE_H
