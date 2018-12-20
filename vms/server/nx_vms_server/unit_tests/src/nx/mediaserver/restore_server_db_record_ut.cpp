#include <gtest/gtest.h>

#include <test_support/mediaserver_launcher.h>
#include <core/resource_management/resource_pool.h>
#include "ec2_thread_pool.h"

namespace nx {
namespace vms::server {
namespace test {

TEST(RestoreServerDbRecord, restoreOwnServerInDatabase)
{
    MediaServerLauncher launcher;
    ASSERT_TRUE(launcher.start());
    ec2::Ec2ThreadPool::instance()->setMaxThreadCount(1);

    auto connection = launcher.serverModule()->ec2Connection();
    auto serverManager = connection->getMediaServerManager(Qn::kSystemAccess);

    QnUuid ownGuid = launcher.serverModule()->commonModule()->moduleGUID();
    ASSERT_EQ(ec2::ErrorCode::ok, serverManager->removeSync(ownGuid));

    auto resourcePool = launcher.serverModule()->resourcePool();
    ASSERT_EQ(1, resourcePool->getAllServers(Qn::ResourceStatus::AnyStatus).size());
}

} // namespace test
} // namespace vms::server
} // namespace nx
