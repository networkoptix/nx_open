#include "p2p_message_bus_test_base.h"

#include <QtCore/QElapsedTimer>

#include <nx/utils/test_support/test_options.h>
#include <nx/utils/log/log.h>
#include <api/common_message_processor.h>
#include <api/global_settings.h>

namespace nx {
namespace p2p {
namespace test {

static const int kMaxInstancesWithTimeManager = 100;

int P2pMessageBusTestBase::m_instanceCounter = 0;
static const QString kP2pTestSystemName(lit("P2pTestSystem"));

void P2pMessageBusTestBase::initResourceTypes(ec2::AbstractECConnection* ec2Connection)
{
    QList<QnResourceTypePtr> resourceTypeList;
    const ec2::ErrorCode errorCode = ec2Connection->getResourceManager(Qn::kSystemAccess)->getResourceTypesSync(&resourceTypeList);
    ASSERT_EQ(ec2::ErrorCode::ok, errorCode);
    qnResTypePool->replaceResourceTypeList(resourceTypeList);
}

void initUsers(ec2::AbstractECConnection* ec2Connection)
{
    ec2::ApiUserDataList users;
    const ec2::ErrorCode errorCode = ec2Connection->getUserManager(Qn::kSystemAccess)->getUsersSync(&users);
    auto messageProcessor = ec2Connection->commonModule()->messageProcessor();
    ASSERT_EQ(ec2::ErrorCode::ok, errorCode);
    for (const auto &user: users)
        messageProcessor->updateResource(user, ec2::NotificationSource::Local);
}

void P2pMessageBusTestBase::createData(
    const Appserver2Ptr& server,
    int camerasCount,
    int propertiesPerCamera,
    int userCount)
{
    const auto connection = server->moduleInstance()->ecConnection();
    auto messageProcessor = server->moduleInstance()->commonModule()->messageProcessor();

    initResourceTypes(connection);
    initUsers(connection);

    const auto settings = connection->commonModule()->globalSettings();
    settings->setSystemName(kP2pTestSystemName);
    settings->setLocalSystemId(guidFromArbitraryData(kP2pTestSystemName));
    settings->setAutoDiscoveryEnabled(false);
    const bool disableTimeManager = m_servers.size() > kMaxInstancesWithTimeManager;
    if (disableTimeManager)
    {
        settings->setTimeSynchronizationEnabled(false);
        settings->setSynchronizingTimeWithInternet(false);
    }

    settings->synchronizeNow();

    //read server list
    ec2::ApiMediaServerDataList mediaServerList;
    ASSERT_EQ(
        ec2::ErrorCode::ok,
        connection->getMediaServerManager(Qn::kSystemAccess)->getServersSync(&mediaServerList));
    for (const auto &mediaServer : mediaServerList)
        messageProcessor->updateResource(mediaServer, ec2::NotificationSource::Local);

    //read camera list
    ec2::ApiCameraDataList cameraList;
    ASSERT_EQ(
        ec2::ErrorCode::ok,
        connection->getCameraManager(Qn::kSystemAccess)->getCamerasSync(&cameraList));
    for (const auto &camera: cameraList)
        messageProcessor->updateResource(camera, ec2::NotificationSource::Local);

    {
        ec2::ApiMediaServerData serverData;
        auto resTypePtr = qnResTypePool->getResourceTypeByName("Server");
        ASSERT_TRUE(!resTypePtr.isNull());
        serverData.typeId = resTypePtr->getId();
        serverData.id = server->moduleInstance()->commonModule()->moduleGUID();
        serverData.authKey = QnUuid::createUuid().toString();
        serverData.name = lm("server %1").arg(serverData.id);
        ASSERT_TRUE(!resTypePtr.isNull());
        serverData.typeId = resTypePtr->getId();

        auto serverManager = connection->getMediaServerManager(Qn::kSystemAccess);
        ASSERT_EQ(ec2::ErrorCode::ok, serverManager->saveSync(serverData));
    }

    std::vector<ec2::ApiUserData> users;

    for (int i = 0; i < userCount; ++i)
    {
        ec2::ApiUserData userData;
        userData.id = QnUuid::createUuid();
        userData.name = lm("user_%1").arg(i);
        userData.isEnabled = true;
        userData.isCloud = false;
        users.push_back(userData);
    }

    std::vector<ec2::ApiCameraData> cameras;
    std::vector<ec2::ApiCameraAttributesData> userAttrs;
    ec2::ApiResourceParamWithRefDataList cameraParams;
    cameras.reserve(camerasCount);
    const auto& moduleGuid = server->moduleInstance()->commonModule()->moduleGUID();
    auto resTypePtr = qnResTypePool->getResourceTypeByName("Camera");
    ASSERT_TRUE(!resTypePtr.isNull());
    for (int i = 0; i < camerasCount; ++i)
    {
        ec2::ApiCameraData cameraData;
        cameraData.typeId = resTypePtr->getId();
        cameraData.parentId = moduleGuid;
        cameraData.vendor = "Invalid camera";
        cameraData.physicalId = QnUuid::createUuid().toString();
        cameraData.name = server->moduleInstance()->endpoint().toString();
        cameraData.id = ec2::ApiCameraData::physicalIdToId(cameraData.physicalId);
        cameras.push_back(std::move(cameraData));

        ec2::ApiCameraAttributesData userAttr;
        userAttr.cameraId = cameraData.id;
        userAttrs.push_back(userAttr);

        for (int j = 0; j < propertiesPerCamera; ++j)
            cameraParams.push_back(ec2::ApiResourceParamWithRefData(cameraData.id, lit("property%1").arg(j), lit("value%1").arg(j)));
    }
    auto userManager = connection->getUserManager(Qn::kSystemAccess);
    auto cameraManager = connection->getCameraManager(Qn::kSystemAccess);
    auto resourceManager = connection->getResourceManager(Qn::kSystemAccess);

    for (const auto& user: users)
        ASSERT_EQ(ec2::ErrorCode::ok, userManager->saveSync(user));
    ASSERT_EQ(ec2::ErrorCode::ok, cameraManager->saveUserAttributesSync(userAttrs));
    ASSERT_EQ(ec2::ErrorCode::ok, resourceManager->saveSync(cameraParams));
    ASSERT_EQ(ec2::ErrorCode::ok, cameraManager->addCamerasSync(cameras));
}

Appserver2Ptr P2pMessageBusTestBase::createAppserver(
    bool keepDbFile,
    quint16 baseTcpPort)
{
    auto tmpDir = nx::utils::TestOptions::temporaryDirectoryPath();
    if (tmpDir.isEmpty())
        tmpDir = QDir::homePath();
    tmpDir += lm("/ec2_server_sync_ut.data%1").arg(m_instanceCounter);
    if (!keepDbFile)
        QDir(tmpDir).removeRecursively();

    Appserver2Ptr result(new Appserver2());
    auto guid = guidFromArbitraryData(lm("guid_hash%1").arg(m_instanceCounter));

    const QString dbFileArg = lit("--dbFile=%1").arg(tmpDir);
    result->addArg(dbFileArg.toStdString().c_str());

    const QString p2pModeArg = lit("--p2pMode=1");
    result->addArg(p2pModeArg.toStdString().c_str());

    const QString instanceArg = lit("--moduleInstance=%1").arg(m_instanceCounter);
    result->addArg(instanceArg.toStdString().c_str());
    const QString guidArg = lit("--moduleGuid=%1").arg(guid.toString());
    result->addArg(guidArg.toStdString().c_str());

    if (baseTcpPort)
    {
        const QString guidArg = lit("--endpoint=0.0.0.0:%1").arg(baseTcpPort + m_instanceCounter);
        result->addArg(guidArg.toStdString().c_str());
    }

    ++m_instanceCounter;

    result->start();
    return result;
}

void P2pMessageBusTestBase::startServers(int count, int keepDbAtServerIndex, quint16 baseTcpPort)
{
    QElapsedTimer t;
    t.restart();
    for (int i = 0; i < count; ++i)
        m_servers.push_back(createAppserver(i == keepDbAtServerIndex, baseTcpPort));

    for (const auto& server: m_servers)
    {
        ASSERT_TRUE(server->waitUntilStarted());
        auto commonModule = server->moduleInstance()->commonModule();
        NX_INFO(this, lit("Server %1 started at url %2")
            .arg(qnStaticCommon->moduleDisplayName(commonModule->moduleGUID()))
            .arg(server->moduleInstance()->endpoint().toString()));
    }
}

void P2pMessageBusTestBase::connectServers(const Appserver2Ptr& srcServer, const Appserver2Ptr& dstServer)
{
    const auto addr = dstServer->moduleInstance()->endpoint();
    auto peerId = dstServer->moduleInstance()->commonModule()->moduleGUID();

    nx::utils::Url url = lit("http://%1:%2/ec2/messageBus").arg(addr.address.toString()).arg(addr.port);
    srcServer->moduleInstance()->ecConnection()->messageBus()->
        addOutgoingConnectionToPeer(peerId, url);
}

// connect servers: 0 --> 1 -->2 --> N
void P2pMessageBusTestBase::sequenceConnect(std::vector<Appserver2Ptr>& servers)
{
    for (int i = 1; i < servers.size(); ++i)
        connectServers(servers[i - 1], servers[i]);
}

// connect servers: 0 --> 1 -->2 --> N --> 0
void P2pMessageBusTestBase::circleConnect(std::vector<Appserver2Ptr>& servers)
{
    sequenceConnect(servers);
    connectServers(servers[servers.size()-1], servers[0]);
}

// connect all servers to each others
void P2pMessageBusTestBase::fullConnect(std::vector<Appserver2Ptr>& servers)
{
    for (int i = 0; i < servers.size(); ++i)
    {
        for (int j = 0; j < servers.size(); ++j)
        {
            if (j == i)
                continue;
            connectServers(servers[j], servers[i]);
        }
    }
}

} // namespace test
} // namespace p2p
} // namespace nx
