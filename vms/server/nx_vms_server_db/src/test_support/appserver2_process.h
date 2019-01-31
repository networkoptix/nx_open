#pragma once

#include <atomic>
#include <memory>

#include <QtCore/QCoreApplication>

#include <nx/network/socket_common.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/stoppable.h>
#include <nx/utils/thread/wait_condition.h>

#include <common/static_common_module.h>
#include <common/common_module.h>
#include <network/http_connection_listener.h>
#include <nx_ec/ec_api.h>

#include "appserver2_http_server.h"
#include <nx/utils/test_support/module_instance_launcher.h>

namespace nx { namespace vms { namespace cloud_integration { class CloudManagerGroup; } } }

namespace ec2 {

class AbstractECConnection;
class LocalConnectionFactory;
class QnSimpleHttpConnectionListener;
class Appserver2Process;

class Appserver2Process:
    public QObject,
    public QnStoppable
{
    Q_OBJECT

public:
    Appserver2Process(int argc, char **argv);
    virtual ~Appserver2Process() = default;

    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);

    int exec();

    QnCommonModule* commonModule() const;
    ec2::AbstractECConnection* ecConnection();
    nx::network::SocketAddress endpoint() const;

    bool createInitialData(const QString& systemName);

    void connectTo(const Appserver2Process* dstServer);

    static void resetInstanceCounter();

signals:
    void beforeStart();

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
    nx::vms::cloud_integration::CloudManagerGroup* m_cloudManagerGroup = nullptr;

    void updateRuntimeData();
    void registerHttpHandlers(ec2::LocalConnectionFactory* ec2ConnectionFactory);
    QnMediaServerResourcePtr addSelfServerResource(
        ec2::AbstractECConnectionPtr ec2Connection,
        int tcpPort);
};

//-------------------------------------------------------------------------------------------------

class Appserver2Launcher:
    public QObject,
    public nx::utils::test::ModuleLauncher<ec2::Appserver2Process>
{
    Q_OBJECT;
    using base_type = ModuleLauncher<ec2::Appserver2Process>;

signals:
    void beforeStart();

public:
    static std::unique_ptr<ec2::Appserver2Launcher> createAppserver(
        bool keepDbFile = false,
        quint16 baseTcpPort = 0);

protected:
    virtual void beforeModuleStart() override
    {
        base_type::beforeModuleStart();
        emit beforeStart();
    }
};

using Appserver2Ptr = std::unique_ptr<Appserver2Launcher>;

} // namespace ec2
