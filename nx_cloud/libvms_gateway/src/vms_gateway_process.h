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
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include "settings.h"

//#define USE_QAPPLICATION


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
#ifdef USE_QAPPLICATION
    public QtService<QtSingleCoreApplication>,
#endif
    public QnStoppable
{
public:
    VmsGatewayProcess( int argc, char **argv );

    //!Implementation of QnStoppable::pleaseStop
    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);

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
#ifndef USE_QAPPLICATION
    QnMutex m_mutex;
    QnWaitCondition m_cond;
#endif

    void initializeLogging(const conf::Settings& settings);
    void registerApiHandlers(
        const conf::Settings& settings,
        nx_http::MessageDispatcher* const msgDispatcher);
    void publicAddressFetched(const QHostAddress& publicAddress);
};

}   //namespace cloud
}   //namespace gateway
}   //namespace nx
