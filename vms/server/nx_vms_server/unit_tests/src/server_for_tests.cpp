#include "server_for_tests.h"

#include <core/resource_management/resource_pool.h>
#include <api/test_api_requests.h>

namespace nx::vms::server::test {

using namespace nx::test;

ServerForTests::ServerForTests():
    id((
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
    storage.url = dataDir() + L'/' + storageName;
    [&](){ NX_TEST_API_POST(this, lit("/ec2/saveStorage"), storage); }();
    return commonModule()->resourcePool()->getResourceById<QnStorageResource>(storage.id);
}

} // namespace nx::vms::server::test
