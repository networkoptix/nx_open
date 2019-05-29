#include "appserver2_process.h"

#include <chrono>
#include <thread>

#include <nx/fusion/serialization/json.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_mod_manager.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/deprecated_settings.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/timer_manager.h>

#include <nx/core/access/access_types.h>
#include <nx/vms/auth/generic_user_data_provider.h>
#include <nx/vms/auth/time_based_nonce_provider.h>
#include <nx/vms/cloud_integration/cloud_manager_group.h>
#include <nx/vms/cloud_integration/vms_cloud_connection_processor.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/utils/detach_server_processor.h>
#include <nx/vms/utils/initial_data_loader.h>
#include <nx/vms/utils/system_merge_processor.h>
#include <nx/vms/utils/setup_system_processor.h>
#include <nx/vms/utils/system_settings_processor.h>

#include <api/app_server_connection.h>
#include <api/common_message_processor.h>
#include <api/global_settings.h>
#include <api/model/configure_reply.h>
#include <api/model/detach_from_cloud_data.h>
#include <api/model/detach_from_cloud_reply.h>
#include <api/model/getnonce_reply.h>
#include <api/model/ping_reply.h>
#include <api/model/setup_cloud_system_data.h>
#include <api/model/setup_local_system_data.h>
#include <api/runtime_info_manager.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <network/tcp_connection_priv.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <rest/server/json_rest_result.h>
#include <rest/server/rest_connection_processor.h>
#include <utils/common/app_info.h>

#include <test_support/resource/test_resource_factory.h>
#include <rest/helper/ping_rest_helper.h>
#include <rest/helper/p2p_statistics.h>
#include <transaction/message_bus_adapter.h>

#include "appserver2_audit_manager.h"
#include "appserver2_process_settings.h"
#include "cloud_integration/cloud_connector.h"
#include "ec2_connection_processor.h"
#include <local_connection_factory.h>
#include <api/model/time_reply.h>
#include <nx/utils/test_support/test_options.h>
#include <rest/handlers/sync_time_rest_handler.h>
#include <nx/vms/network/proxy_connection.h>

static int registerQtResources()
{
    Q_INIT_RESOURCE(nx_vms_server_db);
    return 0;
}

using namespace nx::vms;

namespace ec2 {

int m_instanceCounter = 0;

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
    auto triggerOnStartedEventHandlerGuard = nx::utils::makeScopeGuard(
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

    auto auditManager = std::make_unique<AuditManager>();
    m_commonModule = std::make_unique<QnCommonModule>(false, nx::core::access::Mode::direct);
    m_commonModule->setAuditManager(auditManager.get());

    const auto moduleGuid = settings.moduleGuid();
    m_commonModule->setModuleGUID(
        moduleGuid.isNull() ? QnUuid::createUuid() : moduleGuid);

    QnResourceDiscoveryManager resourceDiscoveryManager(m_commonModule.get());
    // Starting receiving notifications.
    m_commonModule->createMessageProcessor<Appserver2MessageProcessor>(m_commonModule.get());

    updateRuntimeData();

    qnStaticCommon->setModuleShortId(m_commonModule->moduleGUID(), settings.moduleInstance());

    QnSimpleHttpConnectionListener tcpListener(
        m_commonModule.get(),
        QHostAddress::Any,
        settings.endpoint().port,
        QnTcpListener::DEFAULT_MAX_CONNECTIONS,
        true);

    using namespace nx::vms::network;

    std::unique_ptr<ec2::LocalConnectionFactory>
        ec2ConnectionFactory(new ec2::LocalConnectionFactory(
            m_commonModule.get(),
            api::PeerType::server,
            settings.isP2pMode(),
            /*ecDbReadOnly*/ false,
            &tcpListener));

    const nx::utils::Url dbUrl = nx::utils::Url::fromLocalFile(settings.dbFilePath());

    ec2::AbstractECConnectionPtr ec2Connection;
    while (!m_terminated)
    {
        const ec2::ErrorCode errorCode = ec2ConnectionFactory->connectSync(
            dbUrl, nx::vms::api::ClientInfoData(), &ec2Connection);
        if (errorCode == ec2::ErrorCode::ok)
        {
            NX_DEBUG(this, lit("Connected to local EC2"));
            break;
        }

        NX_ERROR(this, lit("Can't connect to local EC2. %1").arg(ec2::toString(errorCode)));
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    ::ec2::CloudConnector cloudConnector(ec2ConnectionFactory->messageBus());

    TimeBasedNonceProvider timeBaseNonceProvider;
    nx::vms::cloud_integration::CloudManagerGroup cloudManagerGroup(
        m_commonModule.get(),
        &timeBaseNonceProvider,
        &cloudConnector,
        std::make_unique<GenericUserDataProvider>(m_commonModule.get()),
        settings.cloudIntegration().delayBeforeSettingMasterFlag);
    m_cloudManagerGroup = &cloudManagerGroup;

    if (!settings.cloudIntegration().cloudDbUrl.isEmpty())
        cloudManagerGroup.setCloudDbUrl(settings.cloudIntegration().cloudDbUrl);

    {
        QnMutexLocker lk(&m_mutex);
        m_tcpListener = &tcpListener;
    }

    if (settings.isAuthDisabled())
    {
        tcpListener.disableAuth();
    }
    else
    {
        tcpListener.setAuthenticator(
            &cloudManagerGroup.userAuthenticator,
            &cloudManagerGroup.authenticationNonceFetcher);
    }

    registerHttpHandlers(ec2ConnectionFactory.get());

    if (!tcpListener.bindToLocalAddress())
        return 1;

    auto server = addSelfServerResource(ec2Connection, tcpListener.getPort());
    nx::vms::utils::loadResourcesFromEcs(
        m_commonModule.get(),
        ec2Connection,
        m_commonModule->messageProcessor(),
        QnMediaServerResourcePtr(),
        []() { return false; });

    m_ecConnection = ec2Connection.get();
    emit beforeStart();

    tcpListener.start();
    m_commonModule->messageProcessor()->init(ec2Connection);
    server->setStatus(Qn::Online);

    processStartResult = true;
    triggerOnStartedEventHandlerGuard.fire();

    m_eventLoop.exec();

    m_tcpListener = nullptr;
    tcpListener.stop();

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

nx::network::SocketAddress Appserver2Process::endpoint() const
{
    QnMutexLocker lk(&m_mutex);
    auto endpoint = m_tcpListener->getLocalEndpoint();
    if (endpoint.address == nx::network::HostAddress::anyHost)
        endpoint.address = nx::network::HostAddress::localhost;
    return endpoint;
}

void Appserver2Process::updateRuntimeData()
{
    nx::vms::api::RuntimeData runtimeData;
    runtimeData.peer.id = m_commonModule->moduleGUID();
    runtimeData.peer.instanceId = m_commonModule->runningInstanceGUID();
    runtimeData.peer.peerType = api::PeerType::server;
    runtimeData.box = QnAppInfo::armBox();
    runtimeData.brand = QnAppInfo::productNameShort();
    runtimeData.platform = QnAppInfo::applicationPlatform();

    runtimeData.hardwareIds << QnUuid::createUuid().toString();
    m_commonModule->runtimeInfoManager()->updateLocalItem(runtimeData);    // initializing localInfo
}

void Appserver2Process::registerHttpHandlers(
    ec2::LocalConnectionFactory* ec2ConnectionFactory)
{
    ec2ConnectionFactory->registerRestHandlers(m_tcpListener->processorPool());

    auto selfInformation = m_commonModule->moduleInformation();
    selfInformation.sslAllowed = false;
    selfInformation.port = m_tcpListener->getPort();
    commonModule()->setModuleInformation(selfInformation);

    auto messageBus = ec2ConnectionFactory->messageBus();

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/moduleInformation",
        [](const nx::network::http::Request&, QnHttpConnectionListener* owner, QnJsonRestResult* result)
        {
            result->setReply(owner->commonModule()->moduleInformation());
            return nx::network::http::StatusCode::ok;
        });
    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/moduleInformationAuthenticated",
        [](const nx::network::http::Request&, QnHttpConnectionListener* owner, QnJsonRestResult* result)
        {
            result->setReply(owner->commonModule()->moduleInformation());
            return nx::network::http::StatusCode::ok;
        });

    auto getNonce = [](const nx::network::http::Request&, QnHttpConnectionListener*, QnJsonRestResult* result)
    {
        QnGetNonceReply reply;
        reply.nonce = QString::number(QDateTime::currentDateTime().toMSecsSinceEpoch(), 16);
        reply.realm = nx::network::AppInfo::realm();
        result->setReply(reply);
        return nx::network::http::StatusCode::ok;
    };
    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/getNonce", getNonce);
    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "web/api/getNonce", getNonce);

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/ping",
        [](const nx::network::http::Request&, QnHttpConnectionListener* owner, QnJsonRestResult* result)
        {
            result->setReply(rest::helper::PingRestHelper::data(owner->commonModule()));
            return nx::network::http::StatusCode::ok;
        });

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/metrics",
        [](const nx::network::http::Request&, QnHttpConnectionListener* owner,
            QnJsonRestResult* result)
        {
            auto metrics = owner->commonModule()->metrics();
            metrics->tcpConnections().totalBytesSent() = nx::network::totalSocketBytesSent();

            const auto& counters = nx::p2p::Connection::sendCounters();
            for (int i = 0; i < (int) nx::p2p::MessageType::counter; ++i)
            {
                auto messageName = toString(nx::p2p::MessageType(i));
                metrics->p2pCounters().dataSentByMessageType()[messageName] = counters[i];
            }

            result->reply = owner->commonModule()->metrics()->toJson(true);
            return nx::network::http::StatusCode::ok;
        });

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", rest::helper::P2pStatistics::kUrlPath,
        [](const nx::network::http::Request&, QnHttpConnectionListener* owner, QnJsonRestResult* result)
        {
            result->setReply(rest::helper::P2pStatistics::data(owner->commonModule()));
            return nx::network::http::StatusCode::ok;
        });

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/backupDatabase",
        [](const nx::network::http::Request&, QnHttpConnectionListener*, QnJsonRestResult*)
        {
            return nx::network::http::StatusCode::ok;
        });

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/configure",
        [](const nx::network::http::Request& request, QnHttpConnectionListener* owner, QnJsonRestResult* result)
        {
            QnConfigureReply reply;
            reply.restartNeeded = false;
            result->setReply(reply);
            ConfigureSystemData data = QJson::deserialized<ConfigureSystemData>(request.messageBody);

            if (!data.localSystemId.isNull() &&
                data.localSystemId != owner->globalSettings()->localSystemId())
            {
                result->setError(QnJsonRestResult::CantProcessRequest, lit("UNSUPPORTED"));
                return nx::network::http::StatusCode::badRequest;
            }
            if (data.rewriteLocalSettings)
            {
                owner->commonModule()->ec2Connection()->setTransactionLogTime(data.tranLogTime);
                owner->globalSettings()->resynchronizeNowSync();
            }
            return nx::network::http::StatusCode::ok;
        });

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/mergeSystems",
        [this](
            const nx::network::http::Request& request,
            QnHttpConnectionListener* /*owner*/,
            QnJsonRestResult* result)
        {
            const auto data = QJson::deserialized<MergeSystemData>(request.messageBody);
            nx::vms::utils::SystemMergeProcessor systemMergeProcessor(m_commonModule.get());
            *result = systemMergeProcessor.merge(
                Qn::kSystemAccess,
                QnAuthSession(),
                data);
            if (result->error == QnRestResult::Error::NoError)
            {
                m_ecConnection.load()->addRemotePeer(
                    systemMergeProcessor.remoteModuleInformation().id,
                    nx::vms::api::PeerType::server,
                    data.url);
            }

            return nx::network::http::StatusCode::ok;
        });

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/setupLocalSystem",
        [this](
            const nx::network::http::Request& request,
            QnHttpConnectionListener* /*owner*/,
            QnJsonRestResult* result)
        {
            auto data = QJson::deserialized<SetupLocalSystemData>(request.messageBody);
            nx::vms::utils::SetupSystemProcessor setupSystemProcessor(m_commonModule.get());
            return setupSystemProcessor.setupLocalSystem(
                QnAuthSession(),
                std::move(data),
                result);
        });

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/setupCloudSystem",
        [this](
            const nx::network::http::Request& request,
            QnHttpConnectionListener* /*owner*/,
            QnJsonRestResult* result)
        {
            const auto data = QJson::deserialized<SetupCloudSystemData>(request.messageBody);
            nx::vms::cloud_integration::VmsCloudConnectionProcessor vmsCloudConnectionProcessor(
                m_commonModule.get(),
                m_cloudManagerGroup);
            return vmsCloudConnectionProcessor.setupCloudSystem(
                QnAuthSession(),
                data,
                result);
    });

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/saveCloudSystemCredentials",
        [this](
            const nx::network::http::Request& request,
            QnHttpConnectionListener* /*owner*/,
            QnJsonRestResult* result)
        {
            const auto data = QJson::deserialized<CloudCredentialsData>(request.messageBody);
            nx::vms::cloud_integration::VmsCloudConnectionProcessor vmsCloudConnectionProcessor(
                m_commonModule.get(),
                m_cloudManagerGroup);
            return vmsCloudConnectionProcessor.bindSystemToCloud(data, result);
        });

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/detachFromCloud",
        [this](
            const nx::network::http::Request& request,
            QnHttpConnectionListener* /*owner*/,
            QnJsonRestResult* result)
        {
            auto data = QJson::deserialized<DetachFromCloudData>(request.messageBody);
            nx::vms::cloud_integration::VmsCloudConnectionProcessor vmsCloudConnectionProcessor(
                m_commonModule.get(),
                m_cloudManagerGroup);
            DetachFromCloudReply reply;
            const auto resultCode = vmsCloudConnectionProcessor.detachFromCloud(data, &reply);
            result->setReply(reply);
            if (resultCode != nx::network::http::StatusCode::ok)
                result->setError(QnRestResult::CantProcessRequest);
            return resultCode;
        });

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP", "api/detachFromSystem",
        [this, messageBus](
            const nx::network::http::Request& request,
            QnHttpConnectionListener* /*owner*/,
            QnJsonRestResult* result)
        {
            messageBus->commonModule()->setStandAloneMode(true);
            messageBus->dropConnections();

            auto data = QJson::deserialized<PasswordData>(request.messageBody);
            nx::vms::utils::DetachServerProcessor detachServerProcessor(
                m_commonModule.get(),
                &m_cloudManagerGroup->connectionManager);
            const auto resultCode = detachServerProcessor.detachServer(result);

            messageBus->commonModule()->setStandAloneMode(false);
            return resultCode;
        });

    m_tcpListener->addHandler<JsonConnectionProcessor>("HTTP",
        nx::vms::time_sync::TimeSyncManager::kTimeSyncUrlPath.mid(1), //< remove '/'
        [](const nx::network::http::Request& request, QnHttpConnectionListener* owner, QnJsonRestResult* result)
    {
        auto timeSyncManager = owner->commonModule()->ec2Connection()->timeSyncManager();
        result->setReply(rest::handlers::SyncTimeRestHandler::execute(timeSyncManager));
        return nx::network::http::StatusCode::ok;
    });

    m_tcpListener->disableAuthForPath("/api/getNonce");
    m_tcpListener->disableAuthForPath("/api/moduleInformation");

    m_tcpListener->addHandler<QnRestConnectionProcessor>("HTTP", "ec2");
    ec2ConnectionFactory->registerTransactionListener(m_tcpListener);

    m_tcpListener->setProxyHandler<nx::vms::network::ProxyConnectionProcessor>(
        &nx::vms::network::ProxyConnectionProcessor::isProxyNeeded,
        ec2ConnectionFactory->serverConnector());
}

void Appserver2Process::resetInstanceCounter()
{
    m_instanceCounter = 0;
}

QnMediaServerResourcePtr Appserver2Process::addSelfServerResource(
    ec2::AbstractECConnectionPtr ec2Connection, int tcpPort)
{
    auto server = QnMediaServerResourcePtr(new QnMediaServerResource(m_commonModule.get()));
    server->setId(commonModule()->moduleGUID());

    m_commonModule->resourcePool()->addResource(server);
    server->setServerFlags(nx::vms::api::SF_HasPublicIP);
    server->setName(QString::number(m_instanceCounter));
    server->setUrl(lit("http://127.0.0.1:%1").arg(tcpPort));

    m_commonModule->bindModuleInformation(server);

    api::MediaServerData apiServer;
    apiServer.id = commonModule()->moduleGUID();
    apiServer.name = server->getName();
    apiServer.authKey = guidFromArbitraryData(apiServer.id.toString() + "authKey").toString();
    apiServer.typeId = nx::vms::api::MediaServerData::kResourceTypeId;

    ec2::fromResourceToApi(server, apiServer);
    if (ec2Connection->getMediaServerManager(Qn::kSystemAccess)->saveSync(apiServer)
        != ec2::ErrorCode::ok)
    {
    }
    return server;
}

bool initResourceTypes(ec2::AbstractECConnection* ec2Connection)
{
    QList<QnResourceTypePtr> resourceTypeList;
    const ec2::ErrorCode errorCode = ec2Connection->getResourceManager(Qn::kSystemAccess)->getResourceTypesSync(&resourceTypeList);
    if (errorCode != ec2::ErrorCode::ok)
        return false;
    qnResTypePool->replaceResourceTypeList(resourceTypeList);
    return true;
}

bool initUsers(ec2::AbstractECConnection* ec2Connection)
{
    nx::vms::api::UserDataList users;
    const ec2::ErrorCode errorCode =
        ec2Connection->getUserManager(Qn::kSystemAccess)->getUsersSync(&users);
    auto messageProcessor = ec2Connection->commonModule()->messageProcessor();
    if (errorCode != ec2::ErrorCode::ok)
        return false;
    for (const auto &user : users)
        messageProcessor->updateResource(user, ec2::NotificationSource::Local);
    return true;
}

bool Appserver2Process::createInitialData(const QString& systemName)
{
    const auto connection = ecConnection();
    if (connection == nullptr)
        return false;
    auto messageProcessor = commonModule()->messageProcessor();

    if (!initResourceTypes(connection))
        return false;
    if (!initUsers(connection))
        return false;

    const auto settings = connection->commonModule()->globalSettings();
    settings->setSystemName(systemName);
    settings->setLocalSystemId(guidFromArbitraryData(systemName));
    settings->setAutoDiscoveryEnabled(false);
    settings->setAutoDiscoveryResponseEnabled(false);

    //read server list
    nx::vms::api::MediaServerDataList mediaServerList;
    auto resultCode =
        connection->getMediaServerManager(Qn::kSystemAccess)->getServersSync(&mediaServerList);
    if (resultCode != ec2::ErrorCode::ok)
        return false;
    for (const auto &mediaServer : mediaServerList)
        messageProcessor->updateResource(mediaServer, ec2::NotificationSource::Local);

    //read camera list
    nx::vms::api::CameraDataList cameraList;
    resultCode =
        connection->getCameraManager(Qn::kSystemAccess)->getCamerasSync(&cameraList);
    if (resultCode != ec2::ErrorCode::ok)
        return false;

    for (const auto &camera : cameraList)
        messageProcessor->updateResource(camera, ec2::NotificationSource::Local);

    auto ownServer = commonModule()->resourcePool()->getResourceById(commonModule()->moduleGUID());
    if (!ownServer)
        return false;
    ownServer->setStatus(Qn::Online);

    return true;
}

void Appserver2Process::connectTo(const Appserver2Process* dstServer)
{
    const auto addr = dstServer->endpoint();
    auto peerId = dstServer->commonModule()->moduleGUID();

    nx::utils::Url url = lit("http://%1:%2/").arg(addr.address.toString()).arg(addr.port);
    ecConnection()->messageBus()-> addOutgoingConnectionToPeer(peerId, nx::vms::api::PeerType::server, url);
}

// ----------------------------- Appserver2Launcher ----------------------------------------------

std::unique_ptr<ec2::Appserver2Launcher> Appserver2Launcher::createAppserver(
    bool keepDbFile,
    quint16 baseTcpPort)
{
    auto tmpDir = nx::utils::TestOptions::temporaryDirectoryPath();
    if (tmpDir.isEmpty())
        tmpDir = QDir::homePath();
    tmpDir += lm("/ec2_server_sync_ut.data%1").arg(m_instanceCounter);
    if (!keepDbFile)
        QDir(tmpDir).removeRecursively();

    Appserver2Ptr result(new Appserver2Launcher());
    auto guid = guidFromArbitraryData(lm("guid_hash%1").arg(m_instanceCounter));

    const QString dbFileArg = lit("--dbFile=%1").arg(tmpDir);
    result->addArg(dbFileArg.toStdString().c_str());

    const QString p2pModeArg = lit("--p2pMode=1");
    result->addArg(p2pModeArg.toStdString().c_str());

    const QString instanceArg = lit("--moduleInstance=%1").arg(m_instanceCounter);
    result->addArg(instanceArg.toStdString().c_str());
    const QString guidArg = lit("--moduleGuid=%1").arg(guid.toString());
    result->addArg(guidArg.toStdString().c_str());

    // Some p2p synchronization tests rely on broken authentication in appserver2.
    result->addArg("--disableAuth");

    if (baseTcpPort)
    {
        const QString guidArg = lit("--endpoint=0.0.0.0:%1").arg(baseTcpPort + m_instanceCounter);
        result->addArg(guidArg.toStdString().c_str());
    }

    ++m_instanceCounter;

    return result;
}

}   // namespace ec2
