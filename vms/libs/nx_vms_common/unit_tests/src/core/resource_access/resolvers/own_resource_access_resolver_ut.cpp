// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/global_permissions_watcher.h>
#include <core/resource_access/resolvers/own_resource_access_resolver.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/common/resource/camera_resource_stub.h>

#include "private/resource_access_resolver_test_fixture.h"

namespace nx::core::access {
namespace test {

using namespace nx::vms::api;

class OwnResourceAccessResolverTest: public ResourceAccessResolverTestFixture
{
public:
    virtual AbstractResourceAccessResolver* createResolver() const override
    {
        return new OwnResourceAccessResolver(manager.get(),
            systemContext()->globalPermissionsWatcher());
    }

    QnUserResourcePtr addAdmin()
    {
        // User must have own admin permissions, not inherited.
        return addUser(NoGroup, kTestUserName, UserType::local, GlobalPermission::powerUser);
    }
};

TEST_F(OwnResourceAccessResolverTest, noAccess)
{
    const auto camera = addCamera();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());
}

TEST_F(OwnResourceAccessResolverTest, notApplicableResource)
{
    const auto user = createUser(NoGroup);
    manager->setOwnResourceAccessMap(kTestSubjectId, {{user->getId(), AccessRight::view}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, user), AccessRights());
}

TEST_F(OwnResourceAccessResolverTest, noDesktopCameraAccess)
{
    const auto user = createUser(NoGroup);
    const auto camera = addDesktopCamera(user);
    manager->setOwnResourceAccessMap(user->getId(), {{camera->getId(), AccessRight::view}});
    ASSERT_EQ(resolver->accessRights(user->getId(), camera), AccessRights());
}

TEST_F(OwnResourceAccessResolverTest, someAccessRights)
{
    const auto camera = addCamera();
    const AccessRights testAccessRights = AccessRight::view;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{camera->getId(), testAccessRights}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), testAccessRights);
}

TEST_F(OwnResourceAccessResolverTest, accessDetails)
{
    const auto camera = addCamera();
    const AccessRights testAccessRights = AccessRight::view;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{camera->getId(), testAccessRights}});

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, camera, AccessRight::view),
        ResourceAccessDetails({{kTestSubjectId, {camera}}}));

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, camera, AccessRight::viewArchive),
        ResourceAccessDetails());
}

TEST_F(OwnResourceAccessResolverTest, notificationSignals)
{
    // Add.
    const auto camera = addCamera();
    manager->setOwnResourceAccessMap(kTestSubjectId, {{camera->getId(), AccessRight::view}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // Change.
    manager->setOwnResourceAccessMap(kTestSubjectId, {{camera->getId(),
        AccessRight::view | AccessRight::viewArchive}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // Remove.
    manager->removeSubjects({kTestSubjectId});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // Clear.
    manager->clear();
    NX_ASSERT_SIGNAL(resourceAccessReset);
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);
}

TEST_F(OwnResourceAccessResolverTest, powerUserAccessToCamera)
{
    const auto camera = addCamera();
    ASSERT_EQ(resolver->accessRights(kPowerUsersGroupId, camera), kFullAccessRights);
}

TEST_F(OwnResourceAccessResolverTest, noPowerUserAccessToDesktopCamera)
{
    const auto other = addUser(NoGroup);
    const auto camera = addDesktopCamera(other);
    ASSERT_EQ(resolver->accessRights(kPowerUsersGroupId, camera), AccessRights());
}

TEST_F(OwnResourceAccessResolverTest, powerUserAccessToWebPage)
{
    const auto webPage = addWebPage();
    ASSERT_EQ(resolver->accessRights(kPowerUsersGroupId, webPage), kViewAccessRights);
}

TEST_F(OwnResourceAccessResolverTest, powerUserAccessToVideowall)
{
    const auto videowall = addVideoWall();
    ASSERT_EQ(resolver->accessRights(kPowerUsersGroupId, videowall), AccessRight::edit);
}

TEST_F(OwnResourceAccessResolverTest, powerUserAccessToSharedLayout)
{
    const auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());
    ASSERT_EQ(resolver->accessRights(kPowerUsersGroupId, layout), kFullAccessRights);
}

TEST_F(OwnResourceAccessResolverTest, adminHasNoAccessToPrivateLayout_5_2)
{
    const auto user = addUser(NoGroup);
    const auto layout = addLayout();
    layout->setParentId(user->getId());
    ASSERT_FALSE(layout->isShared());
    ASSERT_EQ(resolver->accessRights(kAdministratorsGroupId, layout), AccessRights());
}

} // namespace test
} // namespace nx::core::access
