// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/resolvers/abstract_resource_access_resolver.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
#include <nx/vms/common/test_support/test_context.h>

namespace nx::core::access {
namespace test {

using namespace nx::vms::api;
using namespace nx::vms::common::test;

class AbstractResourceAccessResolverTest:
    public ContextBasedTest,
    public AbstractResourceAccessResolver
{
public:
    ResourceAccessMap testAccessMap;

public:
    virtual void TearDown() override
    {
        testAccessMap.clear();
        ContextBasedTest::TearDown();
    }

    virtual ResourceAccessMap resourceAccessMap(const nx::Uuid& /*subjectId*/) const override
    {
        return testAccessMap;
    }

    virtual GlobalPermissions globalPermissions(const nx::Uuid& /*subjectId*/) const override
    {
        return {};
    }

    virtual ResourceAccessDetails accessDetails(
        const nx::Uuid& /*subjectId*/,
        const QnResourcePtr& /*resource*/,
        AccessRight /*accessRight*/) const override
    {
        return {}; //< Out of scope of this test.
    }
};

TEST_F(AbstractResourceAccessResolverTest, notApplicableResourceAccess)
{
    const auto user = addUser(kPowerUsersGroupId);
    testAccessMap[user->getId()] = kFullAccessRights;
    ASSERT_EQ(accessRights({}, user), AccessRights());
}

TEST_F(AbstractResourceAccessResolverTest, notResourceAccessOutOfPool)
{
    testAccessMap = kFullResourceAccessMap;

    const auto camera = createCamera();
    ASSERT_EQ(accessRights({}, camera), AccessRights());

    const auto layout = createLayout();
    testAccessMap[layout->getId()] = kFullAccessRights;
    ASSERT_EQ(accessRights({}, layout), AccessRights());

    const auto videowall = createVideoWall();
    ASSERT_EQ(accessRights({}, videowall), AccessRights());

    const auto cameraInPool = addCamera();
    ASSERT_NE(accessRights({}, cameraInPool), AccessRights());

    resourcePool()->removeResource(cameraInPool);
    ASSERT_EQ(accessRights({}, cameraInPool), AccessRights());
}

TEST_F(AbstractResourceAccessResolverTest, resolveMediaAccess)
{
    testAccessMap[kAllDevicesGroupId] = kFullAccessRights;

    const auto camera = addCamera();
    ASSERT_EQ(accessRights({}, camera), kFullAccessRights);

    const auto layout = addLayout();
    testAccessMap[layout->getId()] = kFullAccessRights; //< Layout access rights must be explicit.

    ASSERT_TRUE(layout->isShared());
    ASSERT_EQ(accessRights({}, layout), kFullAccessRights);
}

TEST_F(AbstractResourceAccessResolverTest, noDesktopCameraAccess)
{
    testAccessMap[kAllDevicesGroupId] = kFullAccessRights;

    const auto user = createUser(kPowerUsersGroupId);
    const auto camera = addDesktopCamera(user);
    ASSERT_EQ(accessRights({}, camera), AccessRights());
}

TEST_F(AbstractResourceAccessResolverTest, resolveViewOnlyAccess)
{
    testAccessMap[kAllWebPagesGroupId] = kFullAccessRights;
    testAccessMap[kAllServersGroupId] = kFullAccessRights;

    const auto webPage = addWebPage();
    ASSERT_EQ(accessRights({}, webPage), kViewAccessRights);

    const auto healthMonitor = addServer();
    ASSERT_EQ(accessRights({}, healthMonitor), kViewAccessRights);
}

TEST_F(AbstractResourceAccessResolverTest, resolveVideoWallAccess)
{
    testAccessMap[kAllVideoWallsGroupId] = kFullAccessRights;

    const auto videoWall = addVideoWall();
    ASSERT_EQ(accessRights({}, videoWall), kFullAccessRights);
}

TEST_F(AbstractResourceAccessResolverTest, resolveNoAccess)
{
    const auto camera = addCamera();
    ASSERT_EQ(accessRights({}, camera), kNoAccessRights);

    const auto layout = addLayout();
    ASSERT_EQ(accessRights({}, layout), kNoAccessRights);

    const auto webPage = addWebPage();
    ASSERT_EQ(accessRights({}, webPage), kNoAccessRights);

    const auto healthMonitor = addServer();
    ASSERT_EQ(accessRights({}, healthMonitor), kNoAccessRights);

    const auto videoWall = addVideoWall();
    ASSERT_EQ(accessRights({}, videoWall), kNoAccessRights);
}

} // namespace test
} // namespace nx::core::access
