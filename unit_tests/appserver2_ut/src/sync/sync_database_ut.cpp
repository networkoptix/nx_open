#include <gtest/gtest.h>

#include <nx/utils/test_support/module_instance_launcher.h>
#include <test_support/appserver2_process.h>
#include <nx_ec/data/api_camera_data.h>
#include <core/resource_access/user_access_data.h>
#include <nx_ec/ec_api.h>

void initResourceTypes(ec2::AbstractECConnection* ec2Connection)
{
    QList<QnResourceTypePtr> resourceTypeList;
    const ec2::ErrorCode errorCode = ec2Connection->getResourceManager(Qn::kSystemAccess)->getResourceTypesSync(&resourceTypeList);
    ASSERT_EQ(ec2::ErrorCode::ok, errorCode);
    qnResTypePool->replaceResourceTypeList(resourceTypeList);
}

using Appserver2 = nx::utils::test::ModuleLauncher<::ec2::Appserver2ProcessPublic>;
using Appserver2Ptr = std::unique_ptr<Appserver2>;

Appserver2Ptr createAppserver()
{
    static int instanceCounter = 0;

    const auto tmpDir =
        (QDir::homePath() + "/ec2_server_sync_ut.data");
    QDir(tmpDir).removeRecursively();

    Appserver2Ptr result(new Appserver2());

    const QString dbFileArg = lit("--dbFile=%1%2").arg(tmpDir).arg(instanceCounter++);
    result->addArg(dbFileArg.toStdString().c_str());

    result->start();
    return result;
}

void createData(const Appserver2Ptr& server)
{
    const auto connection = server->moduleInstance()->ecConnection();

    initResourceTypes(connection);

    ec2::ApiCameraData cameraData;
    auto resTypePtr = qnResTypePool->getResourceTypeByName("Camera");
    ASSERT_TRUE(!resTypePtr.isNull());
    cameraData.typeId = resTypePtr->getId();
    //cameraData.parentId = commonModule()->moduleGUID();
    cameraData.vendor = "Invalid camera";
    cameraData.physicalId = QnUuid::createUuid().toString();
    cameraData.id = ec2::ApiCameraData::physicalIdToId(cameraData.physicalId);

    auto cameraManager = connection->getCameraManager(Qn::kSystemAccess);
    ASSERT_EQ(ec2::ErrorCode::ok, cameraManager->addCameraSync(cameraData));
}

TEST(SympleSyncTest, main)
{
    static const int kInstanceCount = 1;

    std::vector<Appserver2Ptr> servers;
    for (int i = 0; i < kInstanceCount; ++i)
        servers.push_back(createAppserver());
    for(const auto& server: servers)
        ASSERT_TRUE(server->waitUntilStarted());

    for (const auto& server: servers)
        createData(server);
}
