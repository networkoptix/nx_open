#include <gtest/gtest.h>

#include <QtCore/QElapsedTimer>

#include <nx/utils/test_support/module_instance_launcher.h>
#include <test_support/appserver2_process.h>
#include <nx_ec/data/api_camera_data.h>
#include <core/resource_access/user_access_data.h>
#include <nx_ec/ec_api.h>
#include <transaction/message_bus_adapter.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/http/http_client.h>
#include <nx/utils/test_support/test_options.h>

namespace {

static const int kCameraCount = 100;

using Appserver2 = nx::utils::test::ModuleLauncher<::ec2::Appserver2ProcessPublic>;
using Appserver2Ptr = std::unique_ptr<Appserver2>;

static void initResourceTypes(ec2::AbstractECConnection* ec2Connection)
{
    QList<QnResourceTypePtr> resourceTypeList;
    const ec2::ErrorCode errorCode = ec2Connection->getResourceManager(Qn::kSystemAccess)->getResourceTypesSync(&resourceTypeList);
    ASSERT_EQ(ec2::ErrorCode::ok, errorCode);
    qnResTypePool->replaceResourceTypeList(resourceTypeList);
}

static Appserver2Ptr createAppserver()
{
    static int instanceCounter = 0;
    const auto tmpDir = nx::utils::TestOptions::temporaryDirectoryPath() +
        lit("/ec2_server_sync_ut.data") + QString::number(instanceCounter++);
    QDir(tmpDir).removeRecursively();

    Appserver2Ptr result(new Appserver2());

    const QString dbFileArg = lit("--dbFile=%1").arg(tmpDir);
    result->addArg(dbFileArg.toStdString().c_str());

    result->start();
    return result;
}

static void createData(const Appserver2Ptr& server)
{
    const auto connection = server->moduleInstance()->ecConnection();

    initResourceTypes(connection);

    nx_http::HttpClient httpClient;
    httpClient.setUserName("admin");
    httpClient.setUserPassword("admin");

    for (int i = 0; i < kCameraCount; ++i)
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

        auto content = QJson::serialized(cameraData);

        auto addr = server->moduleInstance()->endpoint();

        nx::utils::Url url(lit("http://%1:%2/ec2/saveCamera").arg(addr.address.toString()).arg(addr.port));
        if (!httpClient.doPost(url, "application/json", content))
        {
            break;
        }

        //auto cameraManager = connection->getCameraManager(Qn::kSystemAccess);
        //ASSERT_EQ(ec2::ErrorCode::ok, cameraManager->addCameraSync(cameraData));

    }
}

}

TEST(SympleSyncTest, main)
{
    static const int kInstanceCount = 2;
    static const int kMaxSyncTimeoutMs = 1000 * 5 * 1000;

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

    QElapsedTimer t;
    t.restart();

    for (const auto& server: servers)
        createData(server);

    NX_LOG(lit("Create data time %1").arg(t.elapsed()), cl_logINFO);

    for (int i = 1; i < servers.size(); ++i)
    {
        const auto addr = servers[i]->moduleInstance()->endpoint();
        const auto id = servers[i]->moduleInstance()->commonModule()->moduleGUID();
        nx::utils::Url url = lit("http://%1:%2/ec2/events").arg(addr.address.toString()).arg(addr.port);
        servers[i - 1]->moduleInstance()->ecConnection()->messageBus()->addOutgoingConnectionToPeer(id, url);
    }

    // wait for data sync
    QElapsedTimer timer;
    timer.restart();
    int syncDoneCounter = 0;
    do
    {
        syncDoneCounter = 0;
        for (const auto& server : servers)
        {
            const auto& resPool = server->moduleInstance()->commonModule()->resourcePool();
            const auto& cameraList = resPool->getAllCameras(QnResourcePtr());
            if (cameraList.size() == kCameraCount)
                ++syncDoneCounter;
        }
        ASSERT_TRUE(timer.elapsed() < kMaxSyncTimeoutMs);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (syncDoneCounter != kInstanceCount);
}
