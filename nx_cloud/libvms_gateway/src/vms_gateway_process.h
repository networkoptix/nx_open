/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <atomic>
#include <memory>

#include <qtsinglecoreapplication.h>
#include <qtservice.h>

#include <utils/common/stoppable.h>
#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/http/server/http_stream_socket_server.h>

#include "settings.h"


class QnCommandLineParser;

namespace nx_http
{
    class MessageDispatcher;
}

namespace nx {
namespace cloud {
namespace gateway {

class AuthorizationManager;

class VmsGatewayProcess
:
    public QObject,
    public QtService<QtSingleCoreApplication>,
    public QnStoppable
{
public:
    VmsGatewayProcess( int argc, char **argv );

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
    void registerApiHandlers(nx_http::MessageDispatcher* const msgDispatcher);
};

}   //namespace cloud
}   //namespace gateway
}   //namespace nx
