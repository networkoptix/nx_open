#include "server_for_tests.h"

#include <core/resource_management/resource_pool.h>
#include <api/test_api_requests.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <recorder/storage_manager.h>
#include <nx/utils/test_support/test_options.h>

namespace nx::vms::server::test {

using namespace nx::test;

ServerForTests::ServerForTests(DisabledFeature disabledFeatures):
    MediaServerLauncher(/*tmpDir*/ QString(), /*port*/ 0, disabledFeatures),
    id((
        serverStartTimer.restart(),
        NX_CRITICAL(start()) /* skip critical result */,
        commonModule()->moduleGUID().toSimpleString()))
{
}

std::unique_ptr<ServerForTests> ServerForTests::addServer()
{
    auto server = std::make_unique<ServerForTests>();
    server->connectTo(this);
    return server;
}

QnSharedResourcePointer<resource::test::CameraMock> ServerForTests::addCamera(std::optional<int> id)
{
    QnSharedResourcePointer<resource::test::CameraMock> camera(new resource::test::CameraMock(serverModule(), id));
    camera->setParentId(commonModule()->moduleGUID());
    commonModule()->resourcePool()->addResource(camera);
    return camera;
}

QnStorageResourcePtr ServerForTests::addStorage(const QString& storageName)
{
    vms::api::StorageDataList storages;
    vms::api::StorageData storage;

    storage.id = QnUuid::createUuid();
    storage.name = storageName;
    storage.parentId = commonModule()->moduleGUID();
    storage.spaceLimit = 1000000;
    storage.storageType = "local";
    storage.url = nx::utils::TestOptions::temporaryDirectoryPath() + L'/' + storageName;
    [&](){ NX_TEST_API_POST(this, lit("/ec2/saveStorage"), storage); }();

    auto storageRes = commonModule()->resourcePool()->getResourceById<QnStorageResource>(storage.id);
    while (!serverModule()->normalStorageManager()->hasStorage(storageRes))
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    storageRes->initOrUpdate();
    return storageRes;
}

} // namespace nx::vms::server::test
