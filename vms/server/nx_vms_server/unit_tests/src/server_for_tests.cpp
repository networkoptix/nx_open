#include "server_for_tests.h"

#include <core/resource_management/resource_pool.h>
#include <api/test_api_requests.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <recorder/storage_manager.h>
#include <nx/utils/test_support/test_options.h>
#include <test_support/test_file_storage.h>
#include <test_support/storage_utils.h>

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
     return test_support::addStorage(
        this, nx::utils::TestOptions::temporaryDirectoryPath() + L'/' + storageName,
        QnServer::StoragePool::Normal);
}

} // namespace nx::vms::server::test
