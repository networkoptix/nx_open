#include "appserver2_process.h"

#include <chrono>
#include <thread>

#include <nx/fusion/serialization/json.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_mod_manager.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/settings.h>
#include <nx/utils/timer_manager.h>

#include <nx/core/access/access_types.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/utils/initial_data_loader.h>
#include <nx/vms/utils/server_merge_processor.h>
#include <nx/vms/utils/setup_system_processor.h>
#include <nx/vms/utils/system_settings_processor.h>

#include <api/app_server_connection.h>
#include <api/common_message_processor.h>
#include <api/runtime_info_manager.h>
#include <api/model/getnonce_reply.h>
#include <api/model/configure_reply.h>
#include <api/model/setup_local_system_data.h>
#include <api/global_settings.h>
#include <api/model/ping_reply.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <llutil/hardware_id.h>
#include <network/tcp_connection_priv.h>
#include <nx1/info.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/ec2_lib.h>
#include <rest/server/json_rest_result.h>
#include <rest/server/rest_connection_processor.h>
#include <utils/common/app_info.h>

#include "ec2_connection_processor.h"
#include <test_support/resource/test_resource_factory.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <transaction/message_bus_adapter.h>
#include <rest/helper/ping_rest_helper.h>
#include <rest/helper/p2p_statistics.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/dummy_handler.h>

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

    bool isP2pMode() const
    {
        return m_settings.value("p2pMode", false).toBool();
    }

    int moduleInstance() const
    {
        return m_settings.value("moduleInstance").toInt();
    }

    SocketAddress endpoint() const
    {
        return SocketAddress(m_settings.value("endpoint", "0.0.0.0:0").toString());
    }

    QnUuid moduleGuid() const
    {
        return QnUuid(m_settings.value("moduleGuid").toString());
    }

    bool isAuthDisabled() const
    {
        return m_settings.contains("disableAuth");
    }

private:
    QnSettings m_settings;
    bool m_showHelp;
};

} // namespace conf

//-------------------------------------------------------------------------------------------------

using ProcessorHandler = std::function<
    nx_http::StatusCode::Value(
        const nx_http::Request&,
        QnHttpConnectionListener* owner,
        QnJsonRestResult* result)>;
class JsonConnectionProcessor: public QnTCPConnectionProcessor
{
private:
    ProcessorHandler m_handler;
public:
    JsonConnectionProcessor(
        ProcessorHandler handler,
        QSharedPointer<AbstractStreamSocket> socket,
        QnHttpConnectionListener* owner)
        :
        QnTCPConnectionProcessor(socket, owner),
        m_handler(handler)
    {
    }

    virtual void run() override
    {
        QnJsonRestResult result;
        auto owner = static_cast<QnHttpConnectionListener*>(d_ptr->owner);
        const auto httpStatusCode = m_handler(d_ptr->request, owner, &result);
        d_ptr->response.messageBody = QJson::serialized(result);
        sendResponse(httpStatusCode, Qn::serializationFormatToHttpContentType(Qn::JsonFormat));
    }
};

//-------------------------------------------------------------------------------------------------

QnSimpleHttpConnectionListener::QnSimpleHttpConnectionListener(
    QnCommonModule* commonModule,
    const QHostAddress& address,
    int port,
    int maxConnections,
    bool useSsl)
:
    QnHttpConnectionListener(commonModule, address, port, maxConnections, useSsl)
{
}

QnSimpleHttpConnectionListener::~QnSimpleHttpConnectionListener()
{
    stop();
}

QnTCPConnectionProcessor* QnSimpleHttpConnectionListener::createRequestProcessor(
    QSharedPointer<AbstractStreamSocket> clientSocket)
{
    return new Ec2ConnectionProcessor(clientSocket, this);
}

bool QnSimpleHttpConnectionListener::needAuth(const nx_http::Request& request) const
{
    auto path = request.requestLine.url.path();
    for (const auto& value: m_disableAuthPrefixes)
    {
        if (path.contains(value))
            return false;
    }
    QUrlQuery query(request.requestLine.url.query());
    if (query.hasQueryItem(Qn::URL_QUERY_AUTH_KEY_NAME))
        return false; //< Authenticated by url query
    return QnHttpConnectionListener::needAuth();
}

void QnSimpleHttpConnectionListener::disableAuthForPath(const QString& path)
{
    m_disableAuthPrefixes.insert(path);
}

//-------------------------------------------------------------------------------------------------
// class Appserver2MessageProcessor

class Appserver2MessageProcessor:
    public QnCommonMessageProcessor
{
public:
    Appserver2MessageProcessor(QObject* parent):
        QnCommonMessageProcessor(parent),
        m_factory(new nx::TestResourceFactory())
    {
    }

protected:
    virtual bool canRemoveResource(const QnUuid& id) override
    {
        return id != commonModule()->moduleGUID();
    }

    virtual void removeResourceIgnored(const QnUuid& resourceId) override
    {
        if (resourceId != commonModule()->moduleGUID())
            return;
        QnMediaServerResourcePtr mServer = resourcePool()->getResourceById<QnMediaServerResource>(resourceId);

        ec2::ApiMediaServerData apiServer;
        ec2::fromResourceToApi(mServer, apiServer);
        auto connection = commonModule()->ec2Connection();
        connection->getMediaServerManager(Qn::kSystemAccess)->save(
            apiServer,
            ec2::DummyHandler::instance(),
            &ec2::DummyHandler::onRequestDone);
    }

    virtual void onResourceStatusChanged(
        const QnResourcePtr& /*resource*/,
        Qn::ResourceStatus /*status*/,
        ec2::NotificationSource /*source*/) override
    {
    }

    virtual QnResourceFactory* getResourceFactory() const override
    {
        return nx::TestResourceFactory::instance();
    }

    virtual void updateResource(
        const QnResourcePtr& resource,
        ec2::NotificationSource /*source*/) override
    {
        commonModule()->resourcePool()->addResource(resource);
        if (auto server = resource.dynamicCast<QnMediaServerResource>())
        {
            if (!server->getApiUrl().host().isEmpty())
            {
                commonModule()->ec2Connection()->messageBus()->
                    addOutgoingConnectionToPeer(server->getId(), server->getApiUrl());
            }
        }
    }

protected:
    std::unique_ptr<nx::TestResourceFactory> m_factory;
};

//-------------------------------------------------------------------------------------------------
// class Appserver2Process

Appserver2Process::Appserver2Process(int argc, char** argv):
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
    QnMutexLocker lk(&m_mutex);
    m_terminated = true;
    m_eventLoop.quit();
}

void Appserver2Process::setOnStartedEventHandler(
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler)
{
    m_onStartedEventHandler = std::move(handler);
}

int Appserver2Process::exec()
{
    bool processStartResult = false;
    auto triggerOnStartedEventHandlerGuard = makeScopeGuard(
        [this, &processStartResult]
        {
            if (m_onStartedEventHandler)
                m_onStartedEventHandler(processStartResult);
        });

    registerQtResources();

    conf::Settings settings;
    // Parsing command line arguments.
    settings.load(m_argc, m_argv);
    if (settings.showHelp())
    {
        // TODO: settings.printCmdLineArgsHelp();
        return 0;
    }

    m_commonModule.reset(new QnCommonModule(false, nx::core::access::Mode::direct));
    const auto moduleGuid = settings.moduleGuid();
    m_commonModule->setModuleGUID(
        moduleGuid.isNull() ? QnUuid::createUuid() : moduleGuid);

    QnResourceDiscoveryManager resourceDiscoveryManager(m_commonModule.get());
    // Starting receiving notifications.
    m_commonModule->createMessageProcessor<Appserver2MessageProcessor>();

    updateRuntimeData();

    qnStaticCommon->setModuleShortId(m_commonModule->moduleGUID(), settings.moduleInstance());

    std::unique_ptr<ec2::AbstractECConnectionFactory>
        ec2ConnectionFactory(getConnectionFactory(
            Qn::PT_Server,
            nx::utils::TimerManager::instance(),
            m_commonModule.get(),
            settings.isP2pMode()));

    std::map<QString, QVariant> confParams;
    ec2ConnectionFactory->setConfParams(std::move(confParams));

    const QUrl dbUrl = QUrl::fromLocalFile(settings.dbFilePath());

    ec2::AbstractECConnectionPtr ec2Connection;
    while (!m_terminated)
    {
        const ec2::ErrorCode errorCode = ec2ConnectionFactory->connectSync(
            dbUrl, ec2::ApiClientInfoData(), &ec2Connection);
        if (errorCode == ec2::ErrorCode::ok)
        {
            NX_LOG(lit("Connected to local EC2"), cl_logDEBUG1);
            break;
        }

        NX_LOG(lit("Can't connect to local EC2. %1").arg(ec2::toString(errorCode)), cl_logERROR);
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    addSelfServerResource(ec2Connection);

    nx::vms::utils::loadResourcesFromEcs(
        m_commonModule.get(),
        ec2Connection,
        m_commonModule->messageProcessor(),
        QnMediaServerResourcePtr(),
        []() { return false; });
    
    QnSimpleHttpConnectionListener tcpListener(
        m_commonModule.get(),
        QHostAddress::Any,
        settings.endpoint().port,
        QnTcpListener::DEFAULT_MAX_CONNECTIONS,
        true);

    {
        QnMutexLocker lk(&m_mutex);
        m_tcpListener = &tcpListener;
    }

    if (settings.isAuthDisabled())
        tcpListener.disableAuth();

    registerHttpHandlers(ec2ConnectionFactory.get());

    if (!tcpListener.bindToLocalAddress())
        return 1;

    tcpListener.start();

    m_commonModule->messageProcessor()->init(ec2Connection);
    m_ecConnection = ec2Connection.get();

    processStartResult = true;
    triggerOnStartedEventHandlerGuard.fire();

    m_eventLoop.exec();

    m_tcpListener = nullptr;
    tcpListener.pleaseStop();

    m_commonModule->moduleDiscoveryManager()->stop();
    ec2Connection->stopReceivingNotifications();
    // Must call messageProcessor->init(ec2Connection).
    m_commonModule->messageProcessor()->init(nullptr);
    ec2Connection.reset();
    m_ecConnection = nullptr;

    return 0;
}

ec2::AbstractECConnection* Appserver2Process::ecConnection()
{
    return m_ecConnection.load();
}

QnCommonModule* Appserver2Process::commonModule() const
{
    return m_commonModule.get();
}

SocketAddress Appserver2Process::endpoint() const
{
    QnMutexLocker lk(&m_mutex);
    auto endpoint = m_tcpListener->getLocalEndpoint();
    if (endpoint.address == HostAddress::anyHost)
        endpoint.address = HostAddress::localhost;
    return endpoint;
}

void Appserver2Process::updateRuntimeData()
{
    ec2::ApiRuntimeData runtimeData;
    runtimeData.peer.id = m_commonModule->moduleGUID();
    runtimeData.peer.instanceId = m_commonModule->runningInstanceGUID();
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
    m_commonModule->runtimeInfoManager()->updateLocalItem(runtimeData);    // initializing localInfo
}

void Appserver2Process::registerHttpHandlers(
    ec2::AbstractECConnectionFactory* ec2ConnectionFactory)
{
    ec2ConnectionFactory->registerRestHandlers(m_tcpListener->processorPool());

    auto selfInformation = m_commonModule->moduleInformation();
    selfInformation.sslAllowed = true;
    selfInformation.port = m_tcpListener->getPort();
    commonModule()->setModuleInformation(selfInformation);

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/moduleInformation",
        [](const nx_http::Request&, QnHttpConnectionListener* owner, QnJsonRestResult* result)
        {
            result->setReply(owner->commonModule()->moduleInformation());
            return nx_http::StatusCode::ok;
        });
    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/moduleInformationAuthenticated",
        [](const nx_http::Request&, QnHttpConnectionListener* owner, QnJsonRestResult* result)
        {
            result->setReply(owner->commonModule()->moduleInformation());
            return nx_http::StatusCode::ok;
        });

    auto getNonce = [](const nx_http::Request&, QnHttpConnectionListener*, QnJsonRestResult* result)
    {
        QnGetNonceReply reply;
        reply.nonce = QString::number(QDateTime::currentDateTime().toMSecsSinceEpoch(), 16);
        reply.realm = nx::network::AppInfo::realm();
        result->setReply(reply);
        return nx_http::StatusCode::ok;
    };
    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/getNonce", getNonce);
    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "web/api/getNonce", getNonce);

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/ping",
        [](const nx_http::Request&, QnHttpConnectionListener* owner, QnJsonRestResult* result)
        {
            result->setReply(rest::helper::PingRestHelper::data(owner->commonModule()));
            return nx_http::StatusCode::ok;
        });

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", rest::helper::P2pStatistics::kUrlPath,
        [](const nx_http::Request&, QnHttpConnectionListener* owner, QnJsonRestResult* result)
        {
            result->setReply(rest::helper::P2pStatistics::data(owner->commonModule()));
            return nx_http::StatusCode::ok;
        });

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/backupDatabase",
        [](const nx_http::Request&, QnHttpConnectionListener*, QnJsonRestResult*)
        {
            return nx_http::StatusCode::ok;
        });

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/configure",
        [](const nx_http::Request& request, QnHttpConnectionListener* owner, QnJsonRestResult* result)
        {
            QnConfigureReply reply;
            reply.restartNeeded = false;
            result->setReply(reply);
            ConfigureSystemData data = QJson::deserialized<ConfigureSystemData>(request.messageBody);

            if (!data.localSystemId.isNull() &&
                data.localSystemId != owner->globalSettings()->localSystemId())
            {
                result->setError(QnJsonRestResult::CantProcessRequest, lit("UNSUPPORTED"));
                return nx_http::StatusCode::badRequest;
            }
            if (data.rewriteLocalSettings)
            {
                owner->commonModule()->ec2Connection()->setTransactionLogTime(data.tranLogTime);
                owner->globalSettings()->resynchronizeNowSync();
            }
            return nx_http::StatusCode::ok;
        });

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/mergeSystems",
        [this](
            const nx_http::Request& request,
            QnHttpConnectionListener* owner,
            QnJsonRestResult* result)
        {
            const auto data = QJson::deserialized<MergeSystemData>(request.messageBody);
            nx::vms::utils::SystemMergeProcessor systemMergeProcessor(m_commonModule.get());
            const auto statusCode = systemMergeProcessor.merge(
                Qn::kSystemAccess,
                QnAuthSession(),
                data,
                result);
            if (nx_http::StatusCode::isSuccessCode(statusCode))
            {
                m_ecConnection.load()->addRemotePeer(
                    systemMergeProcessor.remoteModuleInformation().id,
                    data.url);
            }

            return statusCode;
        });

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/setupLocalSystem",
        [this](
            const nx_http::Request& request,
            QnHttpConnectionListener* owner,
            QnJsonRestResult* result)
        {
            auto data = QJson::deserialized<SetupLocalSystemData>(request.messageBody);
            nx::vms::utils::SetupSystemProcessor setupSystemProcessor(m_commonModule.get());
            return setupSystemProcessor.setupLocalSystem(
                QnAuthSession(),
                std::move(data),
                result);
        });

    m_tcpListener->disableAuthForPath("/api/getNonce");
    m_tcpListener->disableAuthForPath("/api/moduleInformation");

    m_tcpListener->addHandler<QnRestConnectionProcessor>("HTTP", "ec2");
    ec2ConnectionFactory->registerTransactionListener(m_tcpListener);
}

void Appserver2Process::addSelfServerResource(
    ec2::AbstractECConnectionPtr ec2Connection)
{
    auto server = QnMediaServerResourcePtr(new QnMediaServerResource(m_commonModule.get()));
    server->setId(commonModule()->moduleGUID());
    m_commonModule->resourcePool()->addResource(server);
    server->setStatus(Qn::Online);

    m_commonModule->bindModuleinformation(server);

    ::ec2::ApiMediaServerData apiServer;
    apiServer.id = commonModule()->moduleGUID();
    ::ec2::fromResourceToApi(server, apiServer);
    if (ec2Connection->getMediaServerManager(Qn::kSystemAccess)->saveSync(apiServer)
        != ec2::ErrorCode::ok)
    {
    }
}

//-------------------------------------------------------------------------------------------------
// class Appserver2ProcessPublic

Appserver2ProcessPublic::Appserver2ProcessPublic(int argc, char **argv):
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

QnCommonModule* Appserver2ProcessPublic::commonModule() const
{
    return m_impl->commonModule();
}

}   // namespace ec2
//}   // namespace nx
