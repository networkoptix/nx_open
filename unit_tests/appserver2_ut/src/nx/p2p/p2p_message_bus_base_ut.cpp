#include "p2p_message_bus_base_ut.h"

#include <QtCore/QElapsedTimer>

#include <nx/utils/test_support/test_options.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace p2p {
namespace test {

void P2pMessageBusTestBase::initResourceTypes(ec2::AbstractECConnection* ec2Connection)
{
    QList<QnResourceTypePtr> resourceTypeList;
    const ec2::ErrorCode errorCode = ec2Connection->getResourceManager(Qn::kSystemAccess)->getResourceTypesSync(&resourceTypeList);
    ASSERT_EQ(ec2::ErrorCode::ok, errorCode);
    qnResTypePool->replaceResourceTypeList(resourceTypeList);
}

void P2pMessageBusTestBase::createData(const Appserver2Ptr& server, int camerasCount, int propertiesPerCamera)
{
    const auto connection = server->moduleInstance()->ecConnection();

    initResourceTypes(connection);

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
    auto cameraManager = connection->getCameraManager(Qn::kSystemAccess);
    auto resourceManager = connection->getResourceManager(Qn::kSystemAccess);
    ASSERT_EQ(ec2::ErrorCode::ok, cameraManager->saveUserAttributesSync(userAttrs));
    ASSERT_EQ(ec2::ErrorCode::ok, resourceManager->saveSync(cameraParams));
    ASSERT_EQ(ec2::ErrorCode::ok, cameraManager->addCamerasSync(cameras));
}

Appserver2Ptr P2pMessageBusTestBase::createAppserver()
{
    static int instanceCounter = 0;

    auto tmpDir = nx::utils::TestOptions::temporaryDirectoryPath();
    if (tmpDir.isEmpty())
        tmpDir = QDir::homePath();
    tmpDir += lm("/ec2_server_sync_ut.data%1").arg(instanceCounter);
    QDir(tmpDir).removeRecursively();

    Appserver2Ptr result(new Appserver2());

    const QString dbFileArg = lit("--dbFile=%1").arg(tmpDir);
    result->addArg(dbFileArg.toStdString().c_str());

    const QString instanceArg = lit("--moduleInstance=%1").arg(instanceCounter);
    result->addArg(instanceArg.toStdString().c_str());

    ++instanceCounter;

    result->start();
    return result;
}

void P2pMessageBusTestBase::startServers(int count)
{
    QElapsedTimer t;
    t.restart();
    for (int i = 0; i < count; ++i)
        m_servers.push_back(createAppserver());

    for (const auto& server: m_servers)
    {
        ASSERT_TRUE(server->waitUntilStarted());
        NX_LOG(lit("Server started at url %1").arg(server->moduleInstance()->endpoint().toString()),
            cl_logINFO);
    }
}

void P2pMessageBusTestBase::connectServers(const Appserver2Ptr& srcServer, const Appserver2Ptr& dstServer)
{
    const auto addr = dstServer->moduleInstance()->endpoint();
    auto peerId = dstServer->moduleInstance()->commonModule()->moduleGUID();

    QUrl url = lit("http://%1:%2/ec2/messageBus").arg(addr.address.toString()).arg(addr.port);
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
