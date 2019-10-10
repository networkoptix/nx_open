#include "server_for_tests.h"

#include <core/resource_management/resource_pool.h>

namespace nx::vms::server::test {

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

} // namespace nx::vms::server::test
