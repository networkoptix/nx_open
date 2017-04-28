#include <gtest/gtest.h>

#include <QtCore/QElapsedTimer>

#include <nx/utils/test_support/module_instance_launcher.h>
#include <test_support/appserver2_process.h>
#include <nx_ec/data/api_camera_data.h>
#include <core/resource_access/user_access_data.h>
#include <nx_ec/ec_api.h>
#include <transaction/transaction_message_bus.h>
#include <transaction/p2p_message_bus.h>
#include <core/resource_management/resource_pool.h>

namespace {

static void initResourceTypes(ec2::AbstractECConnection* ec2Connection)
{
    QList<QnResourceTypePtr> resourceTypeList;
    const ec2::ErrorCode errorCode = ec2Connection->getResourceManager(Qn::kSystemAccess)->getResourceTypesSync(&resourceTypeList);
    ASSERT_EQ(ec2::ErrorCode::ok, errorCode);
    qnResTypePool->replaceResourceTypeList(resourceTypeList);
}

using Appserver2 = nx::utils::test::ModuleLauncher<::ec2::Appserver2ProcessPublic>;
using Appserver2Ptr = std::unique_ptr<Appserver2>;

static Appserver2Ptr createAppserver()
{
    static int instanceCounter = 0;

    const auto tmpDir =
        (QDir::homePath() + "/ec2_server_sync_ut.data") + QString::number(instanceCounter);
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

static void createData(const Appserver2Ptr& server)
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
    {
        ec2::ApiCameraData cameraData;
        auto resTypePtr = qnResTypePool->getResourceTypeByName("Camera");
        ASSERT_TRUE(!resTypePtr.isNull());
        cameraData.typeId = resTypePtr->getId();
        cameraData.parentId = server->moduleInstance()->commonModule()->moduleGUID();
        cameraData.vendor = "Invalid camera";
        cameraData.physicalId = QnUuid::createUuid().toString();
        cameraData.name = server->moduleInstance()->endpoint().toString();
        cameraData.id = ec2::ApiCameraData::physicalIdToId(cameraData.physicalId);

        auto cameraManager = connection->getCameraManager(Qn::kSystemAccess);
        ASSERT_EQ(ec2::ErrorCode::ok, cameraManager->addCameraSync(cameraData));
    }
}

}

TEST(P2pMessageBus, connect)
{
    QnStaticCommonModule staticCommon;

    static const int kInstanceCount = 4;
    static const int kMaxSyncTimeoutMs = 1000 * 20;

    std::vector<Appserver2Ptr> servers;
    for (int i = 0; i < kInstanceCount; ++i)
        servers.push_back(createAppserver());

    for (const auto& server : servers)
    {
        ASSERT_TRUE(server->waitUntilStarted());
        NX_LOG(lit("Server started at url %1").arg(server->moduleInstance()->endpoint().toString()),
            cl_logINFO);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    for (const auto& server: servers)
        createData(server);

    for (int i = 1; i < servers.size(); ++i)
    {
        const auto addr = servers[i]->moduleInstance()->endpoint();
        QUrl url = lit("http://%1:%2/ec2/messageBus").arg(addr.address.toString()).arg(addr.port);
        servers[i - 1]->moduleInstance()->ecConnection()->p2pMessageBus()->
            addOutgoingConnectionToPeer(servers[i]->moduleInstance()->commonModule()->moduleGUID(), url);
    }

    int syncDoneCounter = 0;
    QElapsedTimer timer;
    timer.restart();

    // check subscription
    do
    {
        syncDoneCounter = 0;
        for (const auto& server: servers)
        {
            const auto& connection = server->moduleInstance()->ecConnection();
            const auto& bus = connection->p2pMessageBus();

            for (const auto& serverTo: servers)
            {
                const auto& commonModule = serverTo->moduleInstance()->commonModule();
                ec2::ApiPersistentIdData peer(commonModule->moduleGUID(), commonModule->dbId());
                if (bus->isSubscribedTo(peer))
                    ++syncDoneCounter;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (syncDoneCounter != kInstanceCount*kInstanceCount && timer.elapsed() < kMaxSyncTimeoutMs);

    for (const auto& server : servers)
    {
        const auto& connection = server->moduleInstance()->ecConnection();
        const auto& bus = connection->p2pMessageBus();
        const auto& commonModule = server->moduleInstance()->commonModule();
        for (const auto& serverTo : servers)
        {
            const auto& commonModuleTo = serverTo->moduleInstance()->commonModule();
            ec2::ApiPersistentIdData peer(commonModuleTo->moduleGUID(), commonModuleTo->dbId());
            if (bus->distanceTo(peer) > ec2::kMaxOnlineDistance)
            {
                NX_LOG(lit("Peer %1 has not online distance to peer %2")
                    .arg(qnStaticCommon->moduleDisplayName(commonModule->moduleGUID()))
                    .arg(qnStaticCommon->moduleDisplayName(commonModuleTo->moduleGUID())),
                    cl_logDEBUG1);
            }
            ASSERT_TRUE(bus->distanceTo(peer) <= ec2::kMaxOnlineDistance);
            if (!bus->isSubscribedTo(peer))
            {
                NX_LOG(lit("Peer %1 is not subscribed to peer %2")
                    .arg(qnStaticCommon->moduleDisplayName(commonModule->moduleGUID()))
                    .arg(qnStaticCommon->moduleDisplayName(commonModuleTo->moduleGUID())),
                    cl_logDEBUG1);
            }
            ASSERT_TRUE(bus->isSubscribedTo(peer));
        }
    }

    // wait for data sync
    do
    {
        syncDoneCounter = 0;
        for (const auto& server : servers)
        {
            const auto& resPool = server->moduleInstance()->commonModule()->resourcePool();
            const auto& cameraList = resPool->getAllCameras(QnResourcePtr());
            if (cameraList.size() == kInstanceCount)
                ++syncDoneCounter;
        }
        ASSERT_TRUE(timer.elapsed() < kMaxSyncTimeoutMs);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (syncDoneCounter != kInstanceCount && timer.elapsed() < kMaxSyncTimeoutMs);
}
