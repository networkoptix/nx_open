/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <atomic>
#include <memory>

#include <nx/utils/thread/stoppable.h>
#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include "settings.h"
#include "run_time_options.h"

class QnCommandLineParser;

namespace nx_http
{
    class MessageDispatcher;
}

namespace nx {
namespace cloud {
namespace gateway {

class AuthorizationManager;

class VmsGatewayProcess:
    public QnStoppable
{
public:
    VmsGatewayProcess( int argc, char **argv );

    //!Implementation of QnStoppable::pleaseStop
    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);

    const std::vector<SocketAddress>& httpEndpoints() const;

    void enforceSslFor(const SocketAddress& targetAddress, bool enabled = true);

    int exec();

private:
    conf::RunTimeOptions m_runTimeOptions;

    int m_argc;
    char** m_argv;
    std::atomic<bool> m_terminated;
    int m_timerID;
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> m_startedEventHandler;
    std::vector<SocketAddress> m_httpEndpoints;
    QnMutex m_mutex;
    QnWaitCondition m_cond;

    void registerApiHandlers(
        const conf::Settings& settings,
        const conf::RunTimeOptions& runTimeOptions,
        nx_http::MessageDispatcher* const msgDispatcher);

    void publicAddressFetched(
        const conf::Settings& settings,
        const QString& publicAddress);
};

}   //namespace cloud
}   //namespace gateway
}   //namespace nx
