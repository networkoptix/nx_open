/**********************************************************
* Aug 25, 2016
* a.kolesnikov
***********************************************************/

#include "appserver2_process.h"

#include <chrono>
#include <thread>

#include <nx_ec/ec2_lib.h>
#include <nx/network/http/http_mod_manager.h>
#include <nx/utils/log/log.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/settings.h>

#include <api/app_server_connection.h>
#include <api/common_message_processor.h>
#include <api/runtime_info_manager.h>
#include <common/common_module.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <llutil/hardware_id.h>
#include <network/http_connection_listener.h>
#include <rest/server/rest_connection_processor.h>
#include <utils/common/app_info.h>
#include <nx/utils/scope_guard.h>
#include <nx1/info.h>

#include "ec2_connection_processor.h"

static int registerQtResources()
{
    Q_INIT_RESOURCE(appserver2);
    return 0;
}

//namespace nx {
namespace ec2 {

namespace conf {

class Settings
{
public:
    Settings():
        m_settings(QnAppInfo::organizationNameForSettings(), "Nx Appserver2", "appserver2"),
        m_showHelp(false)
    {
    }

    void load(int argc, char** argv)
    {
        m_settings.parseArgs(argc, (const char**)argv);
    }

    bool showHelp()
    {
        return m_settings.value("help", "not present").toString() != "not present";
    }

    QString dbFilePath() const
    {
        return m_settings.value("dbFile", "ecs.sqlite").toString();
    }

    SocketAddress endpoint() const
    {
        return SocketAddress(m_settings.value("endpoint", "0.0.0.0:0").toString());
    }

private:
    QnSettings m_settings;
    bool m_showHelp;
};

}   // namespace conf


class QnSimpleHttpConnectionListener:
    public QnHttpConnectionListener
{
public:
    QnSimpleHttpConnectionListener(
        const QHostAddress& address,
        int port,
        int maxConnections,
        bool useSsl)
    :
        QnHttpConnectionListener(address, port, maxConnections, useSsl)
    {
    }

    ~QnSimpleHttpConnectionListener()
    {
        stop();
    }

protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(
        QSharedPointer<AbstractStreamSocket> clientSocket) override
    {
        return new Ec2ConnectionProcessor(clientSocket, this);
    }
};

////////////////////////////////////////////////////////////
//// class Appserver2Process
////////////////////////////////////////////////////////////

class Appserver2MessageProcessor:
    public QnCommonMessageProcessor
{
public:
    Appserver2MessageProcessor(
        QnResourcePool* const resourcePool,
        QnResourceDiscoveryManager* resourceDiscoveryManager)
        :
        m_resourcePool(resourcePool),
        m_resourceDiscoveryManager(resourceDiscoveryManager)
    {
    }

protected:
    virtual void onResourceStatusChanged(
        const QnResourcePtr& /*resource*/,
        Qn::ResourceStatus /*status*/,
        ec2::NotificationSource /*source*/) override
    {
    }

    virtual QnResourceFactory* getResourceFactory() const override
    {
        return m_resourceDiscoveryManager;
    }

    virtual void updateResource(
        const QnResourcePtr& resource,
        ec2::NotificationSource /*source*/) override
    {
        m_resourcePool->addResource(resource);
    }

protected:
    QnResourcePool* const m_resourcePool;
    QnResourceDiscoveryManager* m_resourceDiscoveryManager;
};

Appserver2Process::Appserver2Process(int argc, char** argv)
:
    m_argc(argc),
    m_argv(argv),
    m_terminated(false),
    m_ecConnection(nullptr),
    m_tcpListener(nullptr)
{
}

Appserver2Process::~Appserver2Process()
{
}

void Appserver2Process::pleaseStop()
{
    //m_processTerminationEvent.set_value();
    QnMutexLocker lk(&m_mutex);
    m_terminated = true;
    m_cond.wakeAll();
}

void Appserver2Process::setOnStartedEventHandler(
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler)
{
    m_onStartedEventHandler = std::move(handler);
}

int Appserver2Process::exec()
{
    nx::utils::TimerManager timerManager;
    timerManager.start();

    bool processStartResult = false;
    auto triggerOnStartedEventHandlerGuard = makeScopeGuard(
        [this, &processStartResult]
        {
            if (m_onStartedEventHandler)
                m_onStartedEventHandler(processStartResult);
        });

    registerQtResources();

    QnCommonModule commonModule;
    commonModule.setModuleGUID(QnUuid::createUuid());

    QnResourceDiscoveryManager resourceDiscoveryManager;
    // Starting receiving notifications.
    auto messageProcessor = std::make_unique<Appserver2MessageProcessor>(
        QnResourcePool::instance(),
        &resourceDiscoveryManager);

    QnRuntimeInfoManager runtimeInfoManager;

    ec2::ApiRuntimeData runtimeData;
    runtimeData.peer.id = qnCommon->moduleGUID();
    runtimeData.peer.instanceId = qnCommon->runningInstanceGUID();
    runtimeData.peer.peerType = Qn::PT_Server;
    runtimeData.box = QnAppInfo::armBox();
    runtimeData.brand = QnAppInfo::productNameShort();
    runtimeData.platform = QnAppInfo::applicationPlatform();

    if (QnAppInfo::isBpi() || QnAppInfo::isNx1())
    {
        runtimeData.nx1mac = Nx1::getMac();
        runtimeData.nx1serial = Nx1::getSerial();
    }

    runtimeData.hardwareIds << QnUuid::createUuid().toString();
    runtimeInfoManager.updateLocalItem(runtimeData);    // initializing localInfo

    conf::Settings settings;
    //parsing command line arguments
    settings.load(m_argc, m_argv);
    if (settings.showHelp())
    {
        //settings.printCmdLineArgsHelp();
        //TODO
        return 0;
    }

    //initializeLogging(settings);
    std::unique_ptr<ec2::AbstractECConnectionFactory>
        ec2ConnectionFactory(getConnectionFactory(Qn::PT_Server, &timerManager));

    std::map<QString, QVariant> confParams;
    ec2ConnectionFactory->setConfParams(std::move(confParams));

    const QUrl dbUrl = QUrl::fromLocalFile(settings.dbFilePath());

    ec2::AbstractECConnectionPtr ec2Connection;
    //QnConnectionInfo connectInfo;
    while (!m_terminated)
    {
        const ec2::ErrorCode errorCode = ec2ConnectionFactory->connectSync(
            dbUrl, ec2::ApiClientInfoData(), &ec2Connection);
        if (errorCode == ec2::ErrorCode::ok)
        {
            //connectInfo = ec2Connection->connectionInfo();
            NX_LOG(lit("Connected to local EC2"), cl_logWARNING);
            break;
        }

        NX_LOG(lit("Can't connect to local EC2. %1").arg(ec2::toString(errorCode)), cl_logERROR);
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    std::unique_ptr<QnRestProcessorPool> restProcessorPool(new QnRestProcessorPool());
    ec2ConnectionFactory->registerRestHandlers(restProcessorPool.get());

    QnAppServerConnectionFactory appServerConnectionFactory;
    appServerConnectionFactory.setUrl(dbUrl);
    appServerConnectionFactory.setEC2ConnectionFactory(ec2ConnectionFactory.get());
    appServerConnectionFactory.setEc2Connection(ec2Connection);

    nx_http::HttpModManager httpModManager;
    QnSimpleHttpConnectionListener tcpListener(
        QHostAddress::Any,
        settings.endpoint().port,
        QnTcpListener::DEFAULT_MAX_CONNECTIONS,
        true);
    if (!tcpListener.bindToLocalAddress())
    {
        appServerConnectionFactory.setEc2Connection(nullptr);
        appServerConnectionFactory.setEC2ConnectionFactory(nullptr);
        return 1;
    }

    {
        QnMutexLocker lk(&m_mutex);
        m_tcpListener = &tcpListener;
    }

    tcpListener.addHandler<QnRestConnectionProcessor>("HTTP", "ec2");

    tcpListener.start();

    messageProcessor->init(QnAppServerConnectionFactory::getConnection2());
    //ec2Connection->startReceivingNotifications();

    processStartResult = true;
    triggerOnStartedEventHandlerGuard.fire();

    m_ecConnection = ec2Connection.get();

    {
        QnMutexLocker lk(&m_mutex);
        while (!m_terminated)
            m_cond.wait(lk.mutex());

        m_tcpListener = nullptr;
    }

    tcpListener.pleaseStop();

    ec2Connection->stopReceivingNotifications();
    appServerConnectionFactory.setEc2Connection(nullptr);
    appServerConnectionFactory.setEC2ConnectionFactory(nullptr);

    m_ecConnection = nullptr;
    ec2Connection.reset();

    return 0;
}

ec2::AbstractECConnection* Appserver2Process::ecConnection()
{
    return m_ecConnection.load();
}

SocketAddress Appserver2Process::endpoint() const
{
    QnMutexLocker lk(&m_mutex);
    auto endpoint = m_tcpListener->getLocalEndpoint();
    if (endpoint.address == HostAddress::anyHost)
        endpoint.address = HostAddress::localhost;
    return endpoint;
}


////////////////////////////////////////////////////////////
//// class Appserver2ProcessPublic
////////////////////////////////////////////////////////////

Appserver2ProcessPublic::Appserver2ProcessPublic(int argc, char **argv)
    :
    m_impl(new Appserver2Process(argc, argv))
{
}

Appserver2ProcessPublic::~Appserver2ProcessPublic()
{
    delete m_impl;
    m_impl = nullptr;
}

void Appserver2ProcessPublic::pleaseStop()
{
    m_impl->pleaseStop();
}

void Appserver2ProcessPublic::setOnStartedEventHandler(
    nx::utils::MoveOnlyFunc<void(bool)> handler)
{
    m_impl->setOnStartedEventHandler(std::move(handler));
}

int Appserver2ProcessPublic::exec()
{
    return m_impl->exec();
}

const Appserver2Process* Appserver2ProcessPublic::impl() const
{
    return m_impl;
}

ec2::AbstractECConnection* Appserver2ProcessPublic::ecConnection()
{
    return m_impl->ecConnection();
}

SocketAddress Appserver2ProcessPublic::endpoint() const
{
    return m_impl->endpoint();
}

}   // namespace ec2
//}   // namespace nx
