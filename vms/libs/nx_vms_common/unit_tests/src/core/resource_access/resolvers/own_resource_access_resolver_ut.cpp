// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/global_permissions_watcher.h>
#include <core/resource_access/resolvers/own_resource_access_resolver.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>

#include "private/resource_access_resolver_test_fixture.h"
#include "../access_rights_types_testing.h"

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
};

TEST_F(OwnResourceAccessResolverTest, noAccess)
{
    const auto camera = addCamera();
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), AccessRights());
}

TEST_F(OwnResourceAccessResolverTest, notApplicableResource)
{
    const auto user = createUser(GlobalPermission::none);
    manager->setOwnResourceAccessMap(kTestSubjectId, {{user->getId(), AccessRight::viewLive}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, user), AccessRights());
}

TEST_F(OwnResourceAccessResolverTest, noDesktopCameraAccess)
{
    const auto user = createUser(GlobalPermission::none);
    const auto camera = addDesktopCamera(user);
    manager->setOwnResourceAccessMap(user->getId(), {{camera->getId(), AccessRight::viewLive}});
    ASSERT_EQ(resolver->accessRights(user->getId(), camera), AccessRights());
}

TEST_F(OwnResourceAccessResolverTest, someAccessRights)
{
    const auto camera = addCamera();
    const AccessRights testAccessRights = AccessRight::viewLive;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{camera->getId(), testAccessRights}});
    ASSERT_EQ(resolver->accessRights(kTestSubjectId, camera), testAccessRights);
}

TEST_F(OwnResourceAccessResolverTest, accessDetails)
{
    const auto camera = addCamera();
    const AccessRights testAccessRights = AccessRight::viewLive;
    manager->setOwnResourceAccessMap(kTestSubjectId, {{camera->getId(), testAccessRights}});

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, camera, AccessRight::viewLive),
        ResourceAccessDetails({{kTestSubjectId, {camera}}}));

    ASSERT_EQ(resolver->accessDetails(kTestSubjectId, camera, AccessRight::listenToAudio),
        ResourceAccessDetails());
}

TEST_F(OwnResourceAccessResolverTest, notificationSignals)
{
    // Add.
    const auto camera = addCamera();
    manager->setOwnResourceAccessMap(kTestSubjectId, {{camera->getId(), AccessRight::viewLive}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // Change.
    manager->setOwnResourceAccessMap(kTestSubjectId, {{camera->getId(),
        AccessRight::viewLive | AccessRight::listenToAudio}});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // Remove.
    manager->removeSubjects({kTestSubjectId});
    NX_ASSERT_TEST_SUBJECT_CHANGED();

    // Clear.
    manager->clear();
    NX_ASSERT_SIGNAL(resourceAccessReset);
    NX_ASSERT_NO_SIGNAL(resourceAccessChanged);
}

TEST_F(OwnResourceAccessResolverTest, adminAccessToCamera)
{
    const auto admin = addUser(GlobalPermission::admin);
    const auto camera = addCamera();
    ASSERT_EQ(resolver->accessRights(admin->getId(), camera), kAdminAccessRights);
}

TEST_F(OwnResourceAccessResolverTest, noAdminAccessToDesktopCamera)
{
    const auto admin = addUser(GlobalPermission::admin);
    const auto other = addUser(GlobalPermission::viewerPermissions);
    const auto camera = addDesktopCamera(other);
    ASSERT_EQ(resolver->accessRights(admin->getId(), camera), AccessRights());
}

TEST_F(OwnResourceAccessResolverTest, adminAccessToWebPage)
{
    const auto admin = addUser(GlobalPermission::admin);
    const auto webPage = addWebPage();
    ASSERT_EQ(resolver->accessRights(admin->getId(), webPage), kAdminAccessRights);
}

TEST_F(OwnResourceAccessResolverTest, adminAccessToVideowall)
{
    const auto admin = addUser(GlobalPermission::admin);
    const auto videowall = addVideoWall();
    ASSERT_EQ(resolver->accessRights(admin->getId(), videowall), kAdminAccessRights);
}

TEST_F(OwnResourceAccessResolverTest, adminAccessToSharedLayout)
{
    const auto admin = addUser(GlobalPermission::admin);
    const auto layout = addLayout();
    ASSERT_TRUE(layout->isShared());
    ASSERT_EQ(resolver->accessRights(admin->getId(), layout), kAdminAccessRights);
}

TEST_F(OwnResourceAccessResolverTest, adminAccessToPrivateLayout)
{
    const auto admin = addUser(GlobalPermission::admin);
    const auto user = addUser(GlobalPermission::none);
    const auto layout = addLayout();

    layout->setParentId(user->getId());
    ASSERT_FALSE(layout->isShared());

    ASSERT_EQ(resolver->accessRights(admin->getId(), layout), kAdminAccessRights);
}

} // namespace test
} // namespace nx::core::access
