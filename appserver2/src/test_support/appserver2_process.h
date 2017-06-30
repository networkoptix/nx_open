#pragma once

#include <atomic>

#include <QtCore/QCoreApplication>

#include <nx/network/socket_common.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include <nx/utils/thread/stoppable.h>
#include <common/static_common_module.h>
#include <common/common_module.h>

//namespace nx {
namespace ec2 {

class AbstractECConnection;
class QnSimpleHttpConnectionListener;

class Appserver2Process:
    public QnStoppable
{
public:
    Appserver2Process(int argc, char **argv);
    virtual ~Appserver2Process();

    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);

    int exec();

    QnCommonModule* commonModule() const;
    ec2::AbstractECConnection* ecConnection();
    SocketAddress endpoint() const;
private:
    int m_argc;
    char** m_argv;
    std::unique_ptr<QnCommonModule> m_commonModule;
    bool m_terminated;
    nx::utils::promise<void> m_processTerminationEvent;
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> m_onStartedEventHandler;
    std::atomic<AbstractECConnection*> m_ecConnection;
    QnSimpleHttpConnectionListener* m_tcpListener;
    mutable QnMutex m_mutex;
    QEventLoop m_eventLoop;
};


class Appserver2ProcessPublic: public QnStoppable
{
public:
    Appserver2ProcessPublic(int argc, char **argv);
    virtual ~Appserver2ProcessPublic();

    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);

    int exec();

    const Appserver2Process* impl() const;
    ec2::AbstractECConnection* ecConnection();
    SocketAddress endpoint() const;
    QnCommonModule* commonModule() const;
private:
    Appserver2Process* m_impl;
};

}   // namespace ec2
//}   // namespace nx
