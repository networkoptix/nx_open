// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtTest/QSignalSpy>

#include <common/common_globals.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/core/access/access_types.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/std/algorithm.h>
#include <nx/reflect/to_string.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>
#include <nx_ec/data/api_conversion_functions.h>

namespace nx::vms::common::test {

using namespace nx::core::access;
using namespace nx::vms::api;

static constexpr Qn::Permissions kReadGroupPermissions = Qn::ReadPermission;

static constexpr Qn::Permissions kFullGroupPermissions =
    Qn::FullGenericPermissions | Qn::WriteAccessRightsPermission;

static constexpr Qn::Permissions kFullLdapGroupPermissions =
    Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteAccessRightsPermission;

class ResourceAccessManagerTest: public ContextBasedTest
{
protected:
    virtual void TearDown() override
    {
        m_currentUser.clear();
    }

    QnLayoutResourcePtr createLayout(
        Qn::ResourceFlags flags, bool locked = false, const QnUuid& parentId = QnUuid())
    {
        QnLayoutResourcePtr layout(new QnLayoutResource());
        layout->setIdUnsafe(QnUuid::createUuid());
        layout->addFlags(flags);
        layout->setLocked(locked);

        if (!parentId.isNull())
            layout->setParentId(parentId);
        else if (m_currentUser)
            layout->setParentId(m_currentUser->getId());

        return layout;
    }

    void setOwnAccessRights(const QnUuid& subjectId, const ResourceAccessMap accessRights)
    {
        systemContext()->accessRightsManager()->setOwnResourceAccessMap(subjectId, accessRights);
    }

    void logout()
    {
        resourcePool()->removeResources(resourcePool()->getResourcesWithFlag(Qn::remote));
        m_currentUser.clear();
    }

    void loginAsAdministrator()
    {
        loginAs(kAdministratorsGroupId);
    }

    void loginAsCustom()
    {
        loginAs(NoGroup);
    }

    void loginAs(Ids groupIds)
    {
        logout();
        auto user = addUser(groupIds);
        ASSERT_EQ(user->isAdministrator(),
            (groupIds.data == std::vector<QnUuid>{kAdministratorsGroupId}));
        m_currentUser = user;
    }

    Qn::Permissions permissions(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const
    {
        return resourceAccessManager()->permissions(subject, resource);
    }

    Qn::Permissions permissions(const QnResourcePtr& resource) const
    {
        return permissions(m_currentUser, resource);
    }

    bool hasPermissions(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        Qn::Permissions requiredPermissions) const
    {
        return (permissions(subject, resource) & requiredPermissions) == requiredPermissions;
    }

    bool hasPermissions(const QnResourcePtr& resource, Qn::Permissions requiredPermissions) const
    {
        return hasPermissions(m_currentUser, resource, requiredPermissions);
    }

    bool hasAnyPermissions(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        Qn::Permissions requiredPermissions) const
    {
        return (permissions(subject, resource) & requiredPermissions) != 0;
    }

    bool hasAnyPermissions(const QnResourcePtr& resource, Qn::Permissions requiredPermissions) const
    {
        return hasAnyPermissions(m_currentUser, resource, requiredPermissions);
    }

    static const char* invertedDigestAuthorizationEnabled(const QnUserResourcePtr& user)
    {
        return
            user->digestAuthorizationEnabled() ? UserData::kHttpIsDisabledStub : "";
    }

    void checkCanModifyUserDigestAuthorizationEnabled(
        const QnUserResourcePtr& source, QnUserResourcePtr target, bool allowed)
    {
        UserData data;
        ec2::fromResourceToApi(target, data);
        data.digest = invertedDigestAuthorizationEnabled(target);

        auto info = nx::format("check: %1 -> %2", source->getName(), target->getName())
            .toStdString();

        if (allowed)
        {
            ASSERT_TRUE(resourceAccessManager()->canModifyUser(source, target, data)) << info;
            data.digest = invertedDigestAuthorizationEnabled(target);
            ASSERT_TRUE(resourceAccessManager()->canModifyUser(source, target, data)) << info;
        }
        else
        {
            ASSERT_FALSE(resourceAccessManager()->canModifyUser(source, target, data)) << info;
            ec2::fromApiToResource(data, target);
            data.digest = invertedDigestAuthorizationEnabled(target);
            ASSERT_FALSE(resourceAccessManager()->canModifyUser(source, target, data)) << info;
        }
    }

    void checkCanModifyUserDigestAuthorizationEnabled(
        const QnUserResourcePtr& source, bool allowed)
    {
        checkCanModifyUserDigestAuthorizationEnabled(source, m_currentUser, allowed);
    }

    void checkCanGrantPowerUserPermissionsViaInheritance(const QnUserResourcePtr& source,
        QnUserResourcePtr target, const QnUuid& powerUserGroupId, bool allowed)
    {
        UserData data;
        ec2::fromResourceToApi(target, data);
        data.groupIds = target->groupIds();
        data.groupIds.push_back(powerUserGroupId);

        auto info = nx::format("check: %1 -> %2 (inheritance)",
            source->getName(), target->getName()).toStdString();

        if (allowed)
            ASSERT_TRUE(resourceAccessManager()->canModifyUser(source, target, data)) << info;
        else
            ASSERT_FALSE(resourceAccessManager()->canModifyUser(source, target, data)) << info;
    }

    QnUserResourcePtr m_currentUser;
};

// ------------------------------------------------------------------------------------------------
// Checking layouts as Power User

// Check permissions for common layout when the user is logged in as Power User.
TEST_F(ResourceAccessManagerTest, checkLayoutAsPowerUser)
{
    loginAs(kPowerUsersGroupId);

    const auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);

    ASSERT_EQ(permissions(layout), Qn::FullLayoutPermissions);
}

// Check permissions for locked common layout when the user is logged in as Power User.
TEST_F(ResourceAccessManagerTest, checkLockedLayoutAsPowerUser)
{
    loginAs(kPowerUsersGroupId);

    const auto layout = createLayout(Qn::remote, true);
    resourcePool()->addResource(layout);

    const Qn::Permissions forbidden = Qn::RemovePermission
        | Qn::WritePermission
        | Qn::AddRemoveItemsPermission
        | Qn::WriteNamePermission;

    ASSERT_EQ(permissions(layout), Qn::FullLayoutPermissions & ~forbidden);
}

// ------------------------------------------------------------------------------------------------
// Checking own layouts as viewer

// Check permissions for common layout when the user is logged in as viewer.
TEST_F(ResourceAccessManagerTest, checkLayoutAsViewer)
{
    loginAs(kLiveViewersGroupId);

    const auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);

    ASSERT_EQ(permissions(layout), Qn::FullLayoutPermissions);
}

// Check permissions for locked common layout when the user is logged in as viewer.
TEST_F(ResourceAccessManagerTest, checkLockedLayoutAsViewer)
{
    loginAs(kLiveViewersGroupId);

    const auto layout = createLayout(Qn::remote, true);
    resourcePool()->addResource(layout);

    const Qn::Permissions forbidden = Qn::RemovePermission
        | Qn::AddRemoveItemsPermission
        | Qn::WritePermission
        | Qn::WriteNamePermission;

    ASSERT_EQ(permissions(layout), Qn::FullLayoutPermissions & ~forbidden);
}

TEST_F(ResourceAccessManagerTest, checkLockedChanged)
{
    loginAsCustom();
    const auto user = m_currentUser;
    const auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);
    ASSERT_TRUE(hasPermissions(user, layout, Qn::AddRemoveItemsPermission));
    layout->setLocked(true);
    ASSERT_FALSE(hasPermissions(user, layout, Qn::AddRemoveItemsPermission));
}

// ------------------------------------------------------------------------------------------------
// Checking non-own remote layouts

// Check permissions for another viewer's layout when the user is logged in as viewer.
TEST_F(ResourceAccessManagerTest, checkNonOwnViewersLayoutAsViewer)
{
    loginAs(kLiveViewersGroupId);

    const auto anotherUser = addUser(kLiveViewersGroupId, QnResourcePoolTestHelper::kTestUserName2);
    const auto layout = createLayout(Qn::remote, false, anotherUser->getId());
    resourcePool()->addResource(layout);

    ASSERT_EQ(permissions(layout), Qn::NoPermissions);
}

// Check permissions for another viewer's layout when the user is logged in as Power User.
// Since 6.0 Power Users no longer have access to other users' layouts.
TEST_F(ResourceAccessManagerTest, checkNonOwnViewersLayoutAsPowerUser)
{
    loginAs(kPowerUsersGroupId);

    const auto anotherUser = addUser(kLiveViewersGroupId, QnResourcePoolTestHelper::kTestUserName2);
    const auto layout = createLayout(Qn::remote, false, anotherUser->getId());
    resourcePool()->addResource(layout);

    ASSERT_EQ(permissions(layout), Qn::NoPermissions);
}

// Check permissions for another Power User's layout when the user is logged in as Power User.
// Since 6.0 Power Users no longer have access to other users' layouts.
TEST_F(ResourceAccessManagerTest, checkNonOwnPowerUsersLayoutAsPowerUser)
{
    loginAs(kPowerUsersGroupId);

    auto anotherUser = addUser(kPowerUsersGroupId, QnResourcePoolTestHelper::kTestUserName2);
    auto layout = createLayout(Qn::remote, false, anotherUser->getId());
    resourcePool()->addResource(layout);

    ASSERT_EQ(permissions(layout), Qn::NoPermissions);
}

// Check permissions for unknown user's layout when the user is logged in as Power User.
TEST_F(ResourceAccessManagerTest, checkUnknownUsersLayoutAsPowerUser)
{
    loginAs(kPowerUsersGroupId);

    const auto layout = createLayout(Qn::remote, false, QnUuid::createUuid());
    resourcePool()->addResource(layout);

    ASSERT_EQ(permissions(layout), Qn::FullLayoutPermissions);
}

// Check permissions for another power user's layout when the user is logged in as administrator.
TEST_F(ResourceAccessManagerTest, checkNonOwnPowerUsersLayoutAsAdministrator)
{
    loginAsAdministrator();

    const auto anotherUser = addUser(kPowerUsersGroupId, QnResourcePoolTestHelper::kTestUserName2);
    const auto layout = createLayout(Qn::remote, false, anotherUser->getId());
    resourcePool()->addResource(layout);

    ASSERT_EQ(permissions(layout), Qn::FullLayoutPermissions);
}

// ------------------------------------------------------------------------------------------------
// Checking shared layouts

// Check permissions for shared layout when the user is logged in as viewer.
TEST_F(ResourceAccessManagerTest, checkSharedLayoutAsViewer)
{
    loginAs(kLiveViewersGroupId);

    const auto layout = createLayout(Qn::remote);
    layout->setParentId(QnUuid());
    resourcePool()->addResource(layout);

    ASSERT_TRUE(layout->isShared());
    ASSERT_EQ(permissions(layout), Qn::NoPermissions);
}

// Check permissions for shared layout when the user is logged in as Power User.
TEST_F(ResourceAccessManagerTest, checkSharedLayoutAsPowerUser)
{
    loginAs(kPowerUsersGroupId);

    const auto layout = createLayout(Qn::remote);
    layout->setParentId(QnUuid());
    resourcePool()->addResource(layout);

    // PowerUser has full access to shared layouts.
    ASSERT_TRUE(layout->isShared());
    ASSERT_EQ(permissions(layout), Qn::FullLayoutPermissions);
}

// Check permissions for new shared layout when the user is logged in as Power User.
TEST_F(ResourceAccessManagerTest, checkNewSharedLayoutAsPowerUser)
{
    loginAs(kPowerUsersGroupId);

    const auto administrator = addUser(kAdministratorsGroupId);

    const auto layout = createLayout(Qn::remote, false, administrator->getId());
    resourcePool()->addResource(layout);

    // PowerUser cannot modify administrator's layout.
    ASSERT_EQ(permissions(layout), Qn::NoPermissions);

    // Make layout shared.
    layout->setParentId({});

    // PowerUser has full access to shared layouts.
    ASSERT_EQ(permissions(layout), Qn::FullLayoutPermissions);
}

// Custom user loses access to own layout if it becomes shared but not explicitly shared with him.
TEST_F(ResourceAccessManagerTest, checkCustomUserLayoutParentChanged)
{
    loginAsCustom();

    const auto user = m_currentUser;
    const auto layout = createLayout(Qn::remote);

    resourcePool()->addResource(layout);
    ASSERT_EQ(permissions(layout), Qn::FullLayoutPermissions);

    layout->setParentId({});
    ASSERT_EQ(permissions(layout), Qn::NoPermissions);
}

//-------------------------------------------------------------------------------------------------
// Checking intercom layouts

TEST_F(ResourceAccessManagerTest, checkIntercomLayoutPermissions)
{
    loginAsCustom();

    const auto intercomLayout = addIntercom();
    const auto intercomId = intercomLayout->getParentId();
    setOwnAccessRights(m_currentUser->getId(), {{intercomId, AccessRight::view}});

    // Access to intercom grants access to its layout.
    ASSERT_EQ(permissions(intercomLayout), Qn::ReadPermission);

    auto camera = addCamera();
    addToLayout(intercomLayout, camera);

    // Access to intercom layout doesn't grant access to its items.
    ASSERT_EQ(permissions(camera), Qn::NoPermissions);
}

//-------------------------------------------------------------------------------------------------
// Checking videowall-based layouts

// PowerUser can do anything with layout on videowall.
TEST_F(ResourceAccessManagerTest, checkVideowallLayoutAsPowerUser)
{
    loginAs(kPowerUsersGroupId);

    const auto videowall = addVideoWall();
    const auto layout = createLayout(Qn::remote, false, videowall->getId());
    resourcePool()->addResource(layout);

    ASSERT_EQ(permissions(layout), Qn::FullLayoutPermissions);
}

// Videowall-controller can do anything with layout on videowall.
TEST_F(ResourceAccessManagerTest, checkVideowallLayoutAsVideowallController)
{
    loginAsCustom();
    setOwnAccessRights(m_currentUser->getId(), {{kAllVideoWallsGroupId, AccessRight::edit}});

    const auto videowall = addVideoWall();
    const auto layout = createLayout(Qn::remote, false, videowall->getId());
    resourcePool()->addResource(layout);

    ASSERT_EQ(permissions(layout), Qn::FullLayoutPermissions);
}

// Viewer can't do anything with layout on videowall.
TEST_F(ResourceAccessManagerTest, checkVideowallLayoutAsAdvancedViewer)
{
    loginAs(kAdvancedViewersGroupId);

    const auto videowall = addVideoWall();
    const auto layout = createLayout(Qn::remote, false, videowall->getId());
    resourcePool()->addResource(layout);

    ASSERT_EQ(permissions(layout), Qn::NoPermissions);
}

// Locked layouts on videowall still can be removed if the user has control permissions.
TEST_F(ResourceAccessManagerTest, checkVideowallLockedLayout)
{
    const auto group =
        createUserGroup("Videowall group", NoGroup, {{kAllVideoWallsGroupId, AccessRight::edit}});
    loginAs(group.id);

    const auto videowall = addVideoWall();
    const auto layout = createLayout(Qn::remote, true, videowall->getId());
    resourcePool()->addResource(layout);

    const Qn::Permissions forbidden = Qn::AddRemoveItemsPermission
        | Qn::WritePermission
        | Qn::WriteNamePermission;

    ASSERT_EQ(permissions(layout), Qn::FullLayoutPermissions & ~forbidden);
}

TEST_F(ResourceAccessManagerTest, canAccessMyScreenOnVideoWallAsViewer)
{
    loginAs(kLiveViewersGroupId);
    setOwnAccessRights(m_currentUser->getId(), {{kAllVideoWallsGroupId, AccessRight::edit}});

    const auto camera = addDesktopCamera(m_currentUser);

    // User cannot see anybody's else screen.
    ASSERT_FALSE(hasPermissions(m_currentUser, camera, Qn::ViewLivePermission));

    const auto videowall = addVideoWall();
    const auto layout = createLayout(Qn::remote, false, videowall->getId());

    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videowall->items()->addItem(vwitem);

    addToLayout(layout, camera);
    resourcePool()->addResource(layout);

    // Screen is available once it is added to videowall.
    ASSERT_TRUE(hasPermissions(m_currentUser, camera, Qn::ViewLivePermission));
}

TEST_F(ResourceAccessManagerTest, cannotAccessAnyScreenAsAdministrator)
{
    loginAsAdministrator();
    const auto camera = addDesktopCamera(m_currentUser);
    // User cannot see anybody's else screen, even his own.
    ASSERT_FALSE(hasPermissions(m_currentUser, camera, Qn::ViewLivePermission));
}

// User can push it's own screen on a new videowall layout.
TEST_F(ResourceAccessManagerTest, canPushMyScreen)
{
    loginAs(kViewersGroupId);
    setOwnAccessRights(m_currentUser->getId(), {{kAllVideoWallsGroupId, AccessRight::edit}});

    const auto videoWall = addVideoWall();
    const auto ownScreen = addDesktopCamera(m_currentUser);

    const auto layout = QnResourcePoolTestHelper::createLayout();
    layout->setParentId(videoWall->getId());
    addToLayout(layout, ownScreen);

    LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);

    ASSERT_TRUE(resourceAccessManager()->canCreateLayout(m_currentUser, layoutData));
}

// User can push it's own screen on exising videowall layout.
TEST_F(ResourceAccessManagerTest, canPushMyScreenOnExistingLayout)
{
    loginAs(kLiveViewersGroupId);
    setOwnAccessRights(m_currentUser->getId(), {{kAllVideoWallsGroupId, AccessRight::edit}});

    const auto videoWall = addVideoWall();
    const auto ownScreen = addDesktopCamera(m_currentUser);
    const auto layout = addLayout();
    layout->setParentId(videoWall->getId());

    LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);

    nx::vms::api::LayoutItemData item;
    item.id = QnUuid::createUuid();
    item.resourceId = ownScreen->getId();
    layoutData.items.push_back(item);

    ASSERT_TRUE(resourceAccessManager()->canModifyResource(m_currentUser, layout, layoutData));
}

// Pushing screen is allowed only if layout belongs to videowall.
TEST_F(ResourceAccessManagerTest, cannotPushMyScreenNotOnVideoWall)
{
    loginAsAdministrator();

    const auto ownScreen = addDesktopCamera(m_currentUser);
    const auto layout = QnResourcePoolTestHelper::createLayout();
    addToLayout(layout, ownScreen);

    LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);

    // Check shared layout.
    ASSERT_FALSE(resourceAccessManager()->canCreateLayout(m_currentUser, layoutData));

    // Check own layout.
    layoutData.parentId = m_currentUser->getId();
    ASSERT_FALSE(resourceAccessManager()->canCreateLayout(m_currentUser, layoutData));
}

// Pushing screen is allowed only if layout belongs to video wall.
TEST_F(ResourceAccessManagerTest, cannotPushMyScreenOnExistingLayoutNotOnVideowall)
{
    loginAsAdministrator();

    const auto ownScreen = addDesktopCamera(m_currentUser);
    const auto layout = addLayout();

    LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);

    nx::vms::api::LayoutItemData item;
    item.id = QnUuid::createUuid();
    item.resourceId = ownScreen->getId();
    layoutData.items.push_back(item);

    ASSERT_FALSE(resourceAccessManager()->canModifyResource(m_currentUser, layout, layoutData));
}

// Pushing screen is allowed only if there are video wall permissions.
TEST_F(ResourceAccessManagerTest, viewerCannotPushOwnScreen)
{
    loginAs(kViewersGroupId);

    const auto videoWall = addVideoWall();
    const auto ownScreen = addDesktopCamera(m_currentUser);

    const auto layout = QnResourcePoolTestHelper::createLayout();
    layout->setParentId(videoWall->getId());
    addToLayout(layout, ownScreen);

    LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);

    ASSERT_FALSE(resourceAccessManager()->canCreateLayout(m_currentUser, layoutData));
}

// Pushing screen is allowed only if there are video wall permissions.
TEST_F(ResourceAccessManagerTest, viewerCannotPushOwnScreenOnExistingLayout)
{
    loginAs(kViewersGroupId);

    const auto videoWall = addVideoWall();
    const auto ownScreen = addDesktopCamera(m_currentUser);
    const auto layout = addLayout();
    layout->setParentId(videoWall->getId());

    LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);

    nx::vms::api::LayoutItemData item;
    item.id = QnUuid::createUuid();
    item.resourceId = ownScreen->getId();
    layoutData.items.push_back(item);

    ASSERT_FALSE(resourceAccessManager()->canModifyResource(m_currentUser, layout, layoutData));
}

// Pushing screen is allowed only for own screen.
TEST_F(ResourceAccessManagerTest, cannotPushOtherUsersScreen)
{
    loginAsAdministrator();

    const auto anotherUser = addUser(kViewersGroupId);
    const auto videoWall = addVideoWall();
    const auto otherScreen = addDesktopCamera(anotherUser);

    setOwnAccessRights(anotherUser->getId(), {{kAllVideoWallsGroupId, AccessRight::edit}});

    const auto layout = QnResourcePoolTestHelper::createLayout();
    layout->setParentId(videoWall->getId());
    addToLayout(layout, otherScreen);

    LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);

    ASSERT_FALSE(resourceAccessManager()->canCreateLayout(m_currentUser, layoutData));
}

// Pushing screen is allowed only for own screen.
TEST_F(ResourceAccessManagerTest, cannotPushOtherUsersScreenOnExistingLayout)
{
    loginAsAdministrator();

    const auto anotherUser = addUser(kViewersGroupId);
    const auto videoWall = addVideoWall();
    const auto otherScreen = addDesktopCamera(anotherUser);
    const auto layout = addLayout();
    layout->setParentId(videoWall->getId());

    setOwnAccessRights(anotherUser->getId(), {{kAllVideoWallsGroupId, AccessRight::edit}});

    LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);

    nx::vms::api::LayoutItemData item;
    item.id = QnUuid::createUuid();
    item.resourceId = otherScreen->getId();
    layoutData.items.push_back(item);

    ASSERT_FALSE(resourceAccessManager()->canModifyResource(m_currentUser, layout, layoutData));
}

// Pushing screen is allowed only if it is the only camera on layout.
TEST_F(ResourceAccessManagerTest, cannotPushScreenWithCameras)
{
    loginAsAdministrator();

    const auto videoWall = addVideoWall();
    const auto ownScreen = addDesktopCamera(m_currentUser);
    const auto camera = addCamera();

    const auto layout = QnResourcePoolTestHelper::createLayout();
    layout->setParentId(videoWall->getId());
    addToLayout(layout, ownScreen);
    addToLayout(layout, camera);

    LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);

    ASSERT_FALSE(resourceAccessManager()->canCreateLayout(m_currentUser, layoutData));
}

// Pushing screen is allowed only if it is the only camera on layout.
TEST_F(ResourceAccessManagerTest, cannotPushScreenWithCamerasOnExistingLayout)
{
    loginAsAdministrator();

    const auto videoWall = addVideoWall();
    const auto ownScreen = addDesktopCamera(m_currentUser);
    const auto camera = addCamera();
    const auto layout = addLayout();
    layout->setParentId(videoWall->getId());

    LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);

    {
        nx::vms::api::LayoutItemData item;
        item.id = QnUuid::createUuid();
        item.resourceId = ownScreen->getId();
        layoutData.items.push_back(item);
    }
    {
        nx::vms::api::LayoutItemData item;
        item.id = QnUuid::createUuid();
        item.resourceId = camera->getId();
        layoutData.items.push_back(item);
    }

    ASSERT_FALSE(resourceAccessManager()->canModifyResource(m_currentUser, layout, layoutData));
}

// ------------------------------------------------------------------------------------------------
// Checking user access rights

// Check user can edit himself (but cannot rename, remove and change access rights).
TEST_F(ResourceAccessManagerTest, checkUserEditHimself)
{
    loginAsAdministrator();

    const Qn::Permissions forbidden = Qn::WriteNamePermission
        | Qn::RemovePermission
        | Qn::WriteAccessRightsPermission;

    ASSERT_EQ(permissions(m_currentUser, m_currentUser), Qn::FullUserPermissions & ~forbidden);
}

TEST_F(ResourceAccessManagerTest, checkEditDisabledPowerUser)
{
    loginAs(kPowerUsersGroupId);
    const auto otherPowerUser = createUser(kPowerUsersGroupId);
    otherPowerUser->setEnabled(false);
    resourcePool()->addResource(otherPowerUser);
    ASSERT_FALSE(hasPermissions(m_currentUser, otherPowerUser, Qn::WriteAccessRightsPermission));
}

TEST_F(ResourceAccessManagerTest, checkEditDisabledAdministrator)
{
    const auto localAdministrator = createUser(kAdministratorsGroupId, "admin", UserType::local);
    localAdministrator->setEnabled(false);
    resourcePool()->addResource(localAdministrator);

    const auto cloudAdministrator = createUser(
        kAdministratorsGroupId, "user@mail.com", UserType::cloud);
    resourcePool()->addResource(cloudAdministrator);

    ASSERT_FALSE(
        hasPermissions(cloudAdministrator, localAdministrator, Qn::WriteAccessRightsPermission));

    UserData data;
    ec2::fromResourceToApi(localAdministrator, data);
    data.isEnabled = true;
    ASSERT_FALSE(resourceAccessManager()->canModifyUser(
        cloudAdministrator, localAdministrator, data));
}

TEST_F(ResourceAccessManagerTest, checkAdministratorCanNotEditOtherAdministrator)
{
    const auto localAdministrator = createUser(kAdministratorsGroupId, "admin", UserType::local);
    resourcePool()->addResource(localAdministrator);

    const auto cloudAdministrator = createUser(
        kAdministratorsGroupId, "user@mail.com", UserType::cloud);
    resourcePool()->addResource(cloudAdministrator);

    EXPECT_FALSE(hasPermissions(localAdministrator, cloudAdministrator, Qn::WriteDigestPermission));
    EXPECT_TRUE(hasPermissions(cloudAdministrator, localAdministrator, Qn::WriteDigestPermission));

    for (const auto& permission: {
        Qn::WritePasswordPermission, Qn::WriteAccessRightsPermission,
        Qn::WriteEmailPermission, Qn::WriteFullNamePermission})
    {
        const auto label = QJson::serialized(permission).toStdString();
        EXPECT_FALSE(hasPermissions(localAdministrator, cloudAdministrator, permission)) << label;
        EXPECT_FALSE(hasPermissions(cloudAdministrator, localAdministrator, permission)) << label;
    }
}

TEST_F(ResourceAccessManagerTest, checkCanModifyUserDigestAuthorizationEnabledHimself)
{
    loginAs(kLiveViewersGroupId);
    checkCanModifyUserDigestAuthorizationEnabled(m_currentUser, /*allowed*/ false);

    loginAs(kPowerUsersGroupId);
    checkCanModifyUserDigestAuthorizationEnabled(m_currentUser, /*allowed*/ false);

    loginAsAdministrator();
    checkCanModifyUserDigestAuthorizationEnabled(m_currentUser, /*allowed*/ true);

    UserData data;
    ec2::fromResourceToApi(m_currentUser, data);
    data.digest = invertedDigestAuthorizationEnabled(m_currentUser);
    data.name += "_changed";
    ASSERT_FALSE(resourceAccessManager()->canModifyUser(m_currentUser, m_currentUser, data));
}

TEST_F(ResourceAccessManagerTest, checkCanModifyUserDigestAuthorizationEnabledPowerUser)
{
    const auto powerUser = createUser(kPowerUsersGroupId);
    powerUser->removeFlags(Qn::remote); //< Prevent removing of this user by loginAs();
    resourcePool()->addResource(powerUser);

    loginAs(kLiveViewersGroupId);
    checkCanModifyUserDigestAuthorizationEnabled(powerUser, /*allowed*/ true);

    loginAs(kPowerUsersGroupId);
    checkCanModifyUserDigestAuthorizationEnabled(powerUser, /*allowed*/ false);

    loginAsAdministrator();
    checkCanModifyUserDigestAuthorizationEnabled(powerUser, /*allowed*/ false);
}

TEST_F(ResourceAccessManagerTest, checkCanModifyUserDigestAuthorizationEnabled)
{
    const auto administrator = addUser(kAdministratorsGroupId, "administrator");
    const auto cloud = addUser(NoGroup, "cloud", UserType::cloud);
    const auto cloudAdministrator =
        addUser(kAdministratorsGroupId, "cloud administrator", UserType::cloud);
    const auto powerUser = addUser(kPowerUsersGroupId, "powerUser");
    const auto ldap =
        addUser(NoGroup, "ldap", UserType::ldap, GlobalPermission::none, {}, "cn=ldap");
    const auto other = addUser(NoGroup, "other");

    QnUserResourceList users = {administrator, cloudAdministrator, powerUser, cloud, ldap, other};
    QVector<std::pair<QnUserResourcePtr, QnUserResourcePtr>> allowedScenarios = {
        {administrator, administrator},
        {administrator, powerUser},
        {administrator, ldap},
        {administrator, other},
        {cloudAdministrator, administrator},
        {cloudAdministrator, powerUser},
        {cloudAdministrator, ldap},
        {cloudAdministrator, other},
        {powerUser, other},
        {powerUser, ldap}};

    for (const auto source: users)
    {
        for (const auto target: users)
        {
            const bool allowed = allowedScenarios.contains({source, target});
            checkCanModifyUserDigestAuthorizationEnabled(source, target, allowed);
        }
    }
}

TEST_F(ResourceAccessManagerTest, checkCanGrantPowerUserPermissions)
{
    const auto powerUsers = kPowerUsersGroupId;
    const auto powerUsersToo = createUserGroup("Power User Group", kPowerUsersGroupId);
    const auto administrator = addUser(kAdministratorsGroupId, "administrator");
    const auto cloud = addUser(NoGroup, "cloud", UserType::cloud);
    const auto cloudAdministrator = addUser(
        kAdministratorsGroupId, "cloud administrator", UserType::cloud);
    const auto powerUser = addUser(kPowerUsersGroupId, "power user");
    const auto transitivelyInheritedPowerUser = addUser(
        powerUsersToo.id, "transitively inherited power user");
    const auto ldap =
        addUser(NoGroup, "ldap", UserType::ldap, GlobalPermission::none, {}, "cn=ldap");
    const auto other = addUser(NoGroup, "other");

    const QnUserResourceList users = {administrator, cloudAdministrator, powerUser,
        transitivelyInheritedPowerUser, cloud, ldap, other};

    const QVector<std::pair<QnUserResourcePtr, QnUserResourcePtr>> allowedScenariosViaInheritance = {
        {powerUser, powerUser}, //< No change, considered allowed.
        {administrator, powerUser},
        {administrator, transitivelyInheritedPowerUser},
        {administrator, ldap},
        {administrator, other},
        {administrator, cloud},
        {cloudAdministrator, powerUser},
        {cloudAdministrator, transitivelyInheritedPowerUser},
        {cloudAdministrator, cloud},
        {cloudAdministrator, ldap},
        {cloudAdministrator, other}};

    for (const auto editor: users)
    {
        for (const auto editee: users)
        {
            checkCanGrantPowerUserPermissionsViaInheritance(editor, editee, powerUsers,
                allowedScenariosViaInheritance.contains({editor, editee}));
        }
    }
}

TEST_F(ResourceAccessManagerTest, checkWhatUsersCanViewerCreate)
{
    loginAs(kViewersGroupId);

    // Cannot create viewers.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {kViewersGroupId}));

    // Cannot create power users.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::powerUser, {}));

    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {kPowerUsersGroupId}));

    // Cannot create administrators.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::administrator, {}));

    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {kAdministratorsGroupId}));
}

TEST_F(ResourceAccessManagerTest, checkWhatUsersCanPowerUserCreate)
{
    loginAs(kPowerUsersGroupId);

    // Can create viewers.
    ASSERT_TRUE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {kViewersGroupId}));

    // Cannot create power users.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::powerUser, {}));

    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {kPowerUsersGroupId}));

    // Cannot create administrators.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::administrator, {}));

    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {kAdministratorsGroupId}));
}

TEST_F(ResourceAccessManagerTest, checkWhatUsersCanAdministratorCreate)
{
    loginAsAdministrator();

    // Can create viewers.
    ASSERT_TRUE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {kViewersGroupId}));

    // Can create power users.
    ASSERT_TRUE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {kPowerUsersGroupId}));

    // Cannot create power users directly.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::powerUser, {}));

    // Cannot create administrators.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::administrator, {}));

    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {kAdministratorsGroupId}));
}

TEST_F(ResourceAccessManagerTest, checkParentGroupsValidity)
{
    loginAsAdministrator();

    const auto predefinedId = kViewersGroupId;
    const auto customId = createUserGroup("Advanced viewer group", kAdvancedViewersGroupId).id;

    const auto zeroId = QnUuid{};
    const auto unknownId = QnUuid::createUuid();

    ASSERT_TRUE(resourceAccessManager()->canCreateUser(m_currentUser, GlobalPermission::none,
        {predefinedId, customId}));

    ASSERT_FALSE(resourceAccessManager()->canCreateUser(m_currentUser, GlobalPermission::none,
        {predefinedId, zeroId, customId}));

    ASSERT_FALSE(resourceAccessManager()->canCreateUser(m_currentUser, GlobalPermission::none,
        {predefinedId, unknownId, customId}));

    const auto user = addUser(NoGroup);
    UserData userData;
    ec2::fromResourceToApi(user, userData);

    userData.groupIds = {predefinedId, customId};
    ASSERT_TRUE(resourceAccessManager()->canModifyUser(m_currentUser, user, userData));

    userData.groupIds = {predefinedId, zeroId, customId};
    ASSERT_FALSE(resourceAccessManager()->canModifyUser(m_currentUser, user, userData));

    userData.groupIds = {predefinedId, unknownId, customId};
    ASSERT_FALSE(resourceAccessManager()->canModifyUser(m_currentUser, user, userData));
}

// ------------------------------------------------------------------------------------------------
// Checking access rights to user groups.

TEST_F(ResourceAccessManagerTest, checkNoAccessToInvalidGroup)
{
    loginAsAdministrator();

    ASSERT_EQ(resourceAccessManager()->permissions(m_currentUser, UserGroupData{}),
        Qn::Permissions());
}

TEST_F(ResourceAccessManagerTest, checkAdministratorAccessToGroups)
{
    loginAsAdministrator();

    const auto powerUsersGroup = createUserGroup("Power user group", kPowerUsersGroupId);
    const auto nonPowerUsersGroup = createUserGroup("Viewer group", kViewersGroupId);

    ASSERT_EQ(resourceAccessManager()->permissions(m_currentUser, powerUsersGroup),
        kFullGroupPermissions);

    ASSERT_EQ(resourceAccessManager()->permissions(m_currentUser, nonPowerUsersGroup),
        kFullGroupPermissions);
}

TEST_F(ResourceAccessManagerTest, checkPowerUserAccessToGroups)
{
    loginAs(kPowerUsersGroupId);

    const auto powerUsersGroup = createUserGroup("Power user group", kPowerUsersGroupId);
    const auto nonPowerUsersGroup = createUserGroup("Viewer group", kViewersGroupId);

    ASSERT_EQ(resourceAccessManager()->permissions(m_currentUser, powerUsersGroup),
        kReadGroupPermissions);

    ASSERT_EQ(resourceAccessManager()->permissions(m_currentUser, nonPowerUsersGroup),
        kFullGroupPermissions);
}

// Non-power users have no access to user groups, even no Qn::ReadPermission.
TEST_F(ResourceAccessManagerTest, checkNonPowerUserAccessToGroups)
{
    loginAs(kAdvancedViewersGroupId);

    const auto powerUsersGroup = createUserGroup("Power user group", kPowerUsersGroupId);
    const auto nonPowerUsersGroup = createUserGroup("Viewer group", kViewersGroupId);

    ASSERT_EQ(resourceAccessManager()->permissions(m_currentUser, powerUsersGroup),
        Qn::Permissions());

    ASSERT_EQ(resourceAccessManager()->permissions(m_currentUser, nonPowerUsersGroup),
        Qn::Permissions());
}

TEST_F(ResourceAccessManagerTest, checkAccessToLdapGroups)
{
    loginAsAdministrator();

    nx::vms::api::UserGroupData ldapGroup(QnUuid::createUuid(),
        "group name",
        GlobalPermission::none,
        {kPowerUsersGroupId});

    ldapGroup.type = UserType::ldap;
    addOrUpdateUserGroup(ldapGroup);

    ASSERT_EQ(resourceAccessManager()->permissions(m_currentUser, ldapGroup),
        kFullLdapGroupPermissions);
}

TEST_F(ResourceAccessManagerTest, checkAccessToPredefinedGroups)
{
    loginAsAdministrator();

    const auto predefinedGroup = PredefinedUserGroups::find(kLiveViewersGroupId);
    ASSERT_TRUE(!!predefinedGroup);

    ASSERT_EQ(resourceAccessManager()->permissions(m_currentUser, *predefinedGroup),
        kReadGroupPermissions);
}

// ------------------------------------------------------------------------------------------------
// Checking cameras access rights.

// Nobody can view desktop camera footage.
TEST_F(ResourceAccessManagerTest, checkDesktopCameraFootage)
{
    loginAsAdministrator();

    const auto camera = createCamera();
    camera->addFlags(Qn::desktop_camera);
    resourcePool()->addResource(camera);

    ASSERT_FALSE(hasPermissions(m_currentUser, camera,
        Qn::ViewFootagePermission));
}

// Check administrator can remove non-owned desktop camera, but cannot view it.
TEST_F(ResourceAccessManagerTest, checkDesktopCameraRemove)
{
    loginAsAdministrator();

    auto camera = createCamera();
    camera->addFlags(Qn::desktop_camera);
    resourcePool()->addResource(camera);

    ASSERT_TRUE(hasPermissions(camera, Qn::RemovePermission));

    ASSERT_FALSE(hasAnyPermissions(camera, Qn::ReadPermission
        | Qn::ViewContentPermission
        | Qn::ViewLivePermission
        | Qn::ViewFootagePermission));
}

TEST_F(ResourceAccessManagerTest, checkRemoveCameraAsPowerUser)
{
    loginAs(kPowerUsersGroupId);
    const auto target = addCamera();
    ASSERT_TRUE(hasPermissions(target, Qn::RemovePermission));
}

// EditCameras is not enough to be able to remove cameras
TEST_F(ResourceAccessManagerTest, checkRemoveCameraAsEditor)
{
    const auto user = addUser(NoGroup);
    const auto target = addCamera();

    setOwnAccessRights(user->getId(), {{target->getId(),
        AccessRight::view | AccessRight::edit}});

    ASSERT_TRUE(hasPermissions(user, target, Qn::GenericEditPermissions));
    ASSERT_FALSE(hasPermissions(user, target, Qn::RemovePermission));
}

TEST_F(ResourceAccessManagerTest, checkViewCameraPermission)
{
    const auto powerUser = addUser(kPowerUsersGroupId);
    const auto advancedViewer = addUser(kAdvancedViewersGroupId);
    const auto viewer = addUser(kViewersGroupId);
    const auto live = addUser(kLiveViewersGroupId);
    const auto target = addCamera();

    const auto viewLivePermission = Qn::ReadPermission
        | Qn::ViewContentPermission
        | Qn::ViewLivePermission;

    const auto viewPermission = viewLivePermission | Qn::ViewFootagePermission;

    ASSERT_TRUE(hasPermissions(powerUser, target, viewPermission));
    ASSERT_TRUE(hasPermissions(advancedViewer, target, viewPermission));
    ASSERT_TRUE(hasPermissions(viewer, target, viewPermission));
    ASSERT_TRUE(hasPermissions(live, target, viewLivePermission));
    ASSERT_FALSE(hasPermissions(live, target, Qn::ViewFootagePermission));
}

TEST_F(ResourceAccessManagerTest, checkExportCameraPermission)
{
    const auto powerUser = addUser(kPowerUsersGroupId);
    const auto advancedViewer = addUser(kAdvancedViewersGroupId);
    const auto viewer = addUser(kViewersGroupId);
    const auto live = addUser(kLiveViewersGroupId);
    const auto target = addCamera();

    const auto exportPermission = Qn::ReadPermission
        | Qn::ViewContentPermission
        | Qn::ViewFootagePermission
        | Qn::ExportPermission;

    ASSERT_TRUE(hasPermissions(powerUser, target, exportPermission));
    ASSERT_TRUE(hasPermissions(advancedViewer, target, exportPermission));
    ASSERT_TRUE(hasPermissions(viewer, target, exportPermission));
    ASSERT_FALSE(hasPermissions(live, target, exportPermission));
}

TEST_F(ResourceAccessManagerTest, checkUserRemoved)
{
    const auto user = addUser(kPowerUsersGroupId);
    const auto camera = addCamera();

    ASSERT_TRUE(hasPermissions(user, camera, Qn::RemovePermission));
    resourcePool()->removeResource(user);
    ASSERT_FALSE(hasPermissions(user, camera, Qn::RemovePermission));
}

TEST_F(ResourceAccessManagerTest, checkUserRoleChange)
{
    const auto target = addCamera();

    const auto user = addUser(NoGroup);
    ASSERT_FALSE(hasPermissions(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermissions(user, target, Qn::ViewContentPermission));

    user->setGroupIds({kLiveViewersGroupId});
    ASSERT_TRUE(hasPermissions(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermissions(user, target, Qn::ViewContentPermission));
}

TEST_F(ResourceAccessManagerTest, checkUserEnabledChange)
{
    const auto target = addCamera();

    const auto user = addUser(kLiveViewersGroupId);
    ASSERT_TRUE(hasPermissions(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermissions(user, target, Qn::ViewContentPermission));

    user->setEnabled(false);
    ASSERT_FALSE(hasPermissions(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermissions(user, target, Qn::ViewContentPermission));
}

TEST_F(ResourceAccessManagerTest, checkRoleAccessChange)
{
    const auto target = addCamera();

    const auto user = addUser(NoGroup);
    auto group = createUserGroup("test group");

    user->setGroupIds({group.id});
    ASSERT_FALSE(hasPermissions(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermissions(user, target, Qn::ViewContentPermission));

    group.parentGroupIds.push_back(kLiveViewersGroupId);
    addOrUpdateUserGroup(group);

    ASSERT_TRUE(hasPermissions(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermissions(user, target, Qn::ViewContentPermission));
}

TEST_F(ResourceAccessManagerTest, checkInheritedGroupAccessChange)
{
    const auto target = addCamera();

    const auto user = addUser(NoGroup);
    auto parentGroup = createUserGroup("Parent group");
    auto inheritedGroup = createUserGroup("Inherited group", parentGroup.id);

    user->setGroupIds({inheritedGroup.id});
    ASSERT_FALSE(hasPermissions(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermissions(user, target, Qn::ViewContentPermission));

    parentGroup.parentGroupIds = {kLiveViewersGroupId};
    addOrUpdateUserGroup(parentGroup);
    ASSERT_TRUE(hasPermissions(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermissions(user, target, Qn::ViewContentPermission));

    inheritedGroup.parentGroupIds = {};
    addOrUpdateUserGroup(inheritedGroup);
    ASSERT_FALSE(hasPermissions(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermissions(user, target, Qn::ViewContentPermission));

    user->setGroupIds({inheritedGroup.id, parentGroup.id});
    ASSERT_TRUE(hasPermissions(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermissions(user, target, Qn::ViewContentPermission));

    removeUserGroup(parentGroup.id);
    ASSERT_FALSE(hasPermissions(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermissions(user, target, Qn::ViewContentPermission));
}

TEST_F(ResourceAccessManagerTest, checkUserAndRolesCombinedPermissions)
{
    const auto cameraOfParentGroup1 = addCamera();
    const auto cameraOfParentGroup2 = addCamera();
    const auto cameraOfInheritedGroup = addCamera();
    const auto cameraOfUser = addCamera();
    const auto cameraOfNoOne = addCamera();

    const auto parentGroup1 = createUserGroup(
        "Parent group 1", NoGroup, {{cameraOfParentGroup1->getId(), AccessRight::view}});
    EXPECT_FALSE(hasPermissions(parentGroup1, cameraOfUser, Qn::ReadPermission));
    EXPECT_TRUE(hasPermissions(parentGroup1, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(parentGroup1, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(parentGroup1, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(parentGroup1, cameraOfNoOne, Qn::ReadPermission));

    const auto parentGroup2 = createUserGroup(
        "Parent group 2", NoGroup, {{cameraOfParentGroup2->getId(), AccessRight::view}});
    EXPECT_FALSE(hasPermissions(parentGroup2, cameraOfUser, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(parentGroup2, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_TRUE(hasPermissions(parentGroup2, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(parentGroup2, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(parentGroup2, cameraOfNoOne, Qn::ReadPermission));

    auto inheritedGroup = createUserGroup("Inherited group",
        {parentGroup1.id, parentGroup2.id},
        {{cameraOfInheritedGroup->getId(), AccessRight::view}});
    EXPECT_FALSE(hasPermissions(inheritedGroup, cameraOfUser, Qn::ReadPermission));
    EXPECT_TRUE(hasPermissions(inheritedGroup, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_TRUE(hasPermissions(inheritedGroup, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_TRUE(hasPermissions(inheritedGroup, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(inheritedGroup, cameraOfNoOne, Qn::ReadPermission));

    const auto user = addUser(NoGroup, "vasily");
    EXPECT_FALSE(hasPermissions(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfNoOne, Qn::ReadPermission));

    setOwnAccessRights(user->getId(), {{cameraOfUser->getId(), AccessRight::view}});
    EXPECT_TRUE(hasPermissions(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfNoOne, Qn::ReadPermission));

    user->setGroupIds({parentGroup1.id});
    EXPECT_TRUE(hasPermissions(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_TRUE(hasPermissions(user, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfNoOne, Qn::ReadPermission));

    user->setGroupIds({parentGroup1.id, parentGroup2.id});
    EXPECT_TRUE(hasPermissions(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_TRUE(hasPermissions(user, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_TRUE(hasPermissions(user, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfNoOne, Qn::ReadPermission));

    user->setGroupIds({inheritedGroup.id});
    EXPECT_TRUE(hasPermissions(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_TRUE(hasPermissions(user, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_TRUE(hasPermissions(user, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_TRUE(hasPermissions(user, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfNoOne, Qn::ReadPermission));

    inheritedGroup.parentGroupIds = {};
    addOrUpdateUserGroup(inheritedGroup);
    EXPECT_TRUE(hasPermissions(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_TRUE(hasPermissions(user, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfNoOne, Qn::ReadPermission));

    user->setGroupIds({});
    EXPECT_TRUE(hasPermissions(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermissions(user, cameraOfNoOne, Qn::ReadPermission));
}

TEST_F(ResourceAccessManagerTest, checkEditAccessChange)
{
    const auto target = addCamera();

    const auto user = addUser(kLiveViewersGroupId);
    ASSERT_FALSE(hasPermissions(user, target, Qn::SavePermission));

    setOwnAccessRights(user->getId(), {{kAllDevicesGroupId, AccessRight::edit}});
    ASSERT_TRUE(hasPermissions(user, target, Qn::SavePermission));
}

TEST_F(ResourceAccessManagerTest, checkRoleRemoved)
{
    const auto target = addCamera();

    const auto user = addUser(NoGroup);
    const auto group = createUserGroup("Live viewer group", kLiveViewersGroupId);

    user->setGroupIds({group.id});
    ASSERT_TRUE(hasPermissions(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermissions(user, target, Qn::ViewContentPermission));

    removeUserGroup(group.id);
    ASSERT_FALSE(hasPermissions(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermissions(user, target, Qn::ViewContentPermission));
}

TEST_F(ResourceAccessManagerTest, checkCameraOnVideoWall)
{
    loginAs(kPowerUsersGroupId);
    const auto target = addCamera();
    const auto videoWall = addVideoWall();

    const auto user = addUser(NoGroup);
    setOwnAccessRights(user->getId(), {{kAllVideoWallsGroupId, AccessRight::edit}});

    const auto layout = createLayout(Qn::remote, false, videoWall->getId());
    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videoWall->items()->addItem(vwitem);

    LayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);
    resourcePool()->addResource(layout);
    ASSERT_TRUE(hasPermissions(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermissions(user, target, Qn::ViewContentPermission));
}

TEST_F(ResourceAccessManagerTest, checkShareLayoutToRole)
{
    loginAs(kPowerUsersGroupId);

    const auto target = addCamera();

    // Create own layout
    const auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);

    // Create role
    const auto group =
        createUserGroup("Layout group", NoGroup, {{layout->getId(), AccessRight::view}});

    // Create user in role
    const auto user = addUser(group.id, kTestUserName2);

    // Place a camera on it
    LayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    // Share layout to `role`
    layout->setParentId(QnUuid());

    // Make sure user got permissions
    ASSERT_TRUE(hasPermissions(user, target, Qn::ReadPermission));
}

TEST_F(ResourceAccessManagerTest, checkShareLayoutToParentRole)
{
    loginAs(kPowerUsersGroupId);

    const auto target = addCamera();

    // Create own layout
    const auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);

    // Create roles
    const auto parentGroup =
        createUserGroup("Parent group", NoGroup, {{layout->getId(), AccessRight::view}});
    const auto inheritedGroup = createUserGroup("Inherited group", parentGroup.id);

    // Create user in role
    const auto user = addUser(inheritedGroup.id, kTestUserName2);

    // Place a camera on it
    LayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    // Share layout to `parentGroup`
    layout->setParentId(QnUuid());

    // Make sure user got permissions
    ASSERT_TRUE(hasPermissions(user, target, Qn::ReadPermission));
}

// VMAXes without licences:
// Viewing live: temporary allowed
// Viewing footage: temporary allowed
// Export video: temporary allowed
TEST_F(ResourceAccessManagerTest, checkVMaxWithoutLicense)
{
    const auto user = addUser(kPowerUsersGroupId);
    const auto camera = addCamera();

    // Camera is detected as VMax
    camera->markCameraAsVMax();
    ASSERT_TRUE(camera->licenseType() == Qn::LC_VMAX);
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewContentPermission));

    // TODO: Forbid all for VMAX when discussed with management
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewLivePermission));
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewFootagePermission));
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ExportPermission));

    // License enabled
    camera->setScheduleEnabled(true);
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewContentPermission));
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewLivePermission));
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewFootagePermission));
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ExportPermission));
}

// NVRs without licences:
// Viewing live: allowed
// Viewing footage: forbidden
// Export video: forbidden
TEST_F(ResourceAccessManagerTest, checkNvrWithoutLicense)
{
    const auto user = addUser(kPowerUsersGroupId);
    const auto camera = addCamera();

    // Camera is detected as NVR
    camera->markCameraAsNvr();
    ASSERT_TRUE(camera->licenseType() == Qn::LC_Bridge);
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewContentPermission));
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewLivePermission));
    ASSERT_FALSE(hasPermissions(user, camera, Qn::ViewFootagePermission));
    ASSERT_FALSE(hasPermissions(user, camera, Qn::ExportPermission));

    // License enabled
    camera->setScheduleEnabled(true);
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewContentPermission));
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewLivePermission));
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewFootagePermission));
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ExportPermission));
}

// Camera with default auth (if can be changed) must not be viewable.
TEST_F(ResourceAccessManagerTest, checkDefaultAuthCamera)
{
    const auto user = addUser(kPowerUsersGroupId);
    const auto camera = addCamera();

    camera->setCameraCapabilities(
        DeviceCapabilities(DeviceCapability::setUserPassword)
        | DeviceCapability::isDefaultPassword);

    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewContentPermission));
    ASSERT_FALSE(hasPermissions(user, camera, Qn::ViewLivePermission));
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewFootagePermission));
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ExportPermission));

    // Password changed
    camera->setCameraCapabilities(DeviceCapability::setUserPassword);
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewContentPermission));
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewLivePermission));
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewFootagePermission));
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ExportPermission));
}

// Camera with default auth must be viewable if password cannot be changed.
TEST_F(ResourceAccessManagerTest, checkDefaultAuthCameraNonChangeable)
{
    const auto user = addUser(kPowerUsersGroupId);
    const auto camera = addCamera();

    camera->setCameraCapabilities(DeviceCapability::isDefaultPassword);
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewContentPermission));
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewLivePermission));
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ViewFootagePermission));
    ASSERT_TRUE(hasPermissions(user, camera, Qn::ExportPermission));
}

// ------------------------------------------------------------------------------------------------
// Checking servers access rights

// PowerUser must have full permissions for servers.
TEST_F(ResourceAccessManagerTest, checkServerAsPowerUser)
{
    loginAs(kPowerUsersGroupId);
    const auto server = addServer();
    ASSERT_EQ(permissions(server), Qn::FullServerPermissions & ~Qn::RemovePermission);
}

// Custom users can't view health monitors by default.
TEST_F(ResourceAccessManagerTest, checkServerAsCustom)
{
    loginAsCustom();
    const auto server = addServer();
    ASSERT_FALSE(hasPermissions(server, Qn::ViewContentPermission));
}

// User with live viewer permissions can view health monitors by default.
TEST_F(ResourceAccessManagerTest, checkServerAsLiveViewer)
{
    loginAs(kLiveViewersGroupId);
    const auto server = addServer();
    ASSERT_EQ(permissions(server), Qn::ReadPermission | Qn::ViewContentPermission);
}

// ------------------------------------------------------------------------------------------------
// Checking storages access rights.

// PowerUser must have full permissions for storages.
TEST_F(ResourceAccessManagerTest, checkStoragesAsPowerUser)
{
    loginAs(kPowerUsersGroupId);

    const auto server = addServer();
    const auto storage = addStorage(server);
    ASSERT_EQ(permissions(storage), Qn::ReadWriteSavePermission | Qn::RemovePermission);
}

// Non-power users should not have access to storages.
TEST_F(ResourceAccessManagerTest, checkStoragesAsCustom)
{
    loginAsCustom();

    const auto server = addServer();
    const auto storage = addStorage(server);
    ASSERT_EQ(permissions(storage), Qn::NoPermissions);
}

TEST_F(ResourceAccessManagerTest, checkStoragesAsLiveViewer)
{
    loginAs(kLiveViewersGroupId);

    const auto server = addServer();
    const auto storage = addStorage(server);
    ASSERT_EQ(permissions(storage), Qn::NoPermissions);
}

// ------------------------------------------------------------------------------------------------
// Checking videowall access rights.

// PowerUser must have full permissions for videowalls.
TEST_F(ResourceAccessManagerTest, checkVideowallAsPowerUser)
{
    loginAs(kPowerUsersGroupId);
    const auto videowall = addVideoWall();
    ASSERT_EQ(permissions(videowall), Qn::FullGenericPermissions);
}

// Videowall control user must have almost full permissions for videowalls.
TEST_F(ResourceAccessManagerTest, checkVideowallAsController)
{
    loginAsCustom();
    setOwnAccessRights(m_currentUser->getId(), {{kAllVideoWallsGroupId, AccessRight::edit}});

    const auto videowall = addVideoWall();
    ASSERT_EQ(permissions(videowall), Qn::ReadPermission | Qn::GenericEditPermissions);
    ASSERT_FALSE(hasPermissions(videowall, Qn::RemovePermission));
}

// Videowall is inaccessible for default user.
TEST_F(ResourceAccessManagerTest, checkVideowallAsViewer)
{
    loginAs(kAdvancedViewersGroupId);
    const auto videowall = addVideoWall();
    ASSERT_EQ(permissions(videowall), Qn::NoPermissions);
}

// ------------------------------------------------------------------------------------------------
// Checking web page access rights.

// PowerUser must have full permissions for web pages.
TEST_F(ResourceAccessManagerTest, checkWebPageAsPowerUser)
{
    loginAs(kPowerUsersGroupId);
    const auto webPage = addWebPage();
    ASSERT_EQ(permissions(webPage), Qn::FullWebPagePermissions);
}

// Web page is read-only for viewers.
TEST_F(ResourceAccessManagerTest, checkWebPageAsViewer)
{
    loginAs(kViewersGroupId);
    const auto webPage = addWebPage();
    ASSERT_EQ(permissions(webPage), Qn::ReadPermission | Qn::ViewContentPermission);
}

// Web page is accessible to custom users only if explicitly shared.
TEST_F(ResourceAccessManagerTest, checkWebPageAsCustom)
{
    loginAsCustom();

    const auto webPage = addWebPage();
    ASSERT_EQ(permissions(webPage), Qn::NoPermissions);

    // Share explicitly.
    setOwnAccessRights(m_currentUser->getId(), {{webPage->getId(), AccessRight::view}});
    ASSERT_EQ(permissions(webPage), Qn::ReadPermission | Qn::ViewContentPermission);

    // Share via all web pages virtual group.
    setOwnAccessRights(m_currentUser->getId(), {{kAllWebPagesGroupId, AccessRight::view}});
    ASSERT_EQ(permissions(webPage), Qn::ReadPermission | Qn::ViewContentPermission);
}

// ------------------------------------------------------------------------------------------------
// Permissions by id.

TEST_F(ResourceAccessManagerTest, checkPermissionsById)
{
    loginAsAdministrator();

    const auto camera = addCamera();
    const auto server = addServer();
    const auto user = addUser(kViewersGroupId);
    const auto group = createUserGroup("Viewer group", kViewersGroupId);

    ASSERT_EQ(
        resourceAccessManager()->permissions(m_currentUser, camera),
        resourceAccessManager()->permissions(m_currentUser, camera->getId()));

    ASSERT_EQ(
        resourceAccessManager()->permissions(m_currentUser, server),
        resourceAccessManager()->permissions(m_currentUser, server->getId()));

    ASSERT_EQ(
        resourceAccessManager()->permissions(m_currentUser, user),
        resourceAccessManager()->permissions(m_currentUser, user->getId()));

    ASSERT_EQ(
        resourceAccessManager()->permissions(m_currentUser, group),
        resourceAccessManager()->permissions(m_currentUser, group.id));
}

// ------------------------------------------------------------------------------------------------
// AccessRights related checks

TEST_F(ResourceAccessManagerTest, accessRightsIndependency)
{
    loginAsCustom();

    const auto camera = addCamera();
    const auto layout = addLayout();
    addToLayout(layout, camera);

    ASSERT_EQ(permissions(camera), Qn::NoPermissions);

    static const std::map<AccessRight, Qn::Permissions> kTestAccessRights{
        {AccessRight::view, Qn::ViewContentPermission | Qn::ViewLivePermission},
        {AccessRight::viewArchive, Qn::ViewContentPermission | Qn::ViewFootagePermission},
        {AccessRight::viewBookmarks, Qn::ViewBookmarksPermission},
        {AccessRight::manageBookmarks, Qn::ManageBookmarksPermission},
        {AccessRight::userInput, Qn::UserInputPermissions},
        {AccessRight::edit, Qn::GenericEditPermissions}};

    for (const auto [testAccessRight, expectedPermissions]: kTestAccessRights)
    {
        setOwnAccessRights(m_currentUser->getId(), {{layout->getId(), testAccessRight}});
        EXPECT_EQ(permissions(camera), Qn::ReadPermission | expectedPermissions);
    }

    setOwnAccessRights(m_currentUser->getId(), {{layout->getId(), AccessRight::exportArchive}});
    EXPECT_TRUE(hasPermissions(m_currentUser, camera, Qn::ExportPermission));
    EXPECT_FALSE(hasPermissions(m_currentUser, camera, Qn::ViewFootagePermission));

    setOwnAccessRights(m_currentUser->getId(), {{layout->getId(), AccessRight::viewArchive}});
    EXPECT_FALSE(hasPermissions(m_currentUser, camera, Qn::ExportPermission));
    EXPECT_TRUE(hasPermissions(m_currentUser, camera, Qn::ViewFootagePermission));
}

// ------------------------------------------------------------------------------------------------
// Permission dependency changes

class PermissionDependenciesTest: public ResourceAccessManagerTest
{
protected:
    virtual void SetUp() override
    {
        ResourceAccessManagerTest::SetUp();

        permissionsDependencyChanged.reset(new QSignalSpy(resourceAccessManager(),
            &QnResourceAccessManager::permissionsDependencyChanged));
    }

    virtual void TearDown() override
    {
        permissionsDependencyChanged.reset();
        ResourceAccessManagerTest::TearDown();
    }

public:
    std::unique_ptr<QSignalSpy> permissionsDependencyChanged;
};

#define CHECK_NO_DEPENDENCY_NOTIFICATIONS() \
    ASSERT_TRUE(permissionsDependencyChanged->empty());

#define CHECK_DEPENDENCY_NOTIFICATION(resources) { \
    ASSERT_EQ(permissionsDependencyChanged->size(), 1); \
    const auto args = permissionsDependencyChanged->takeFirst(); \
    ASSERT_EQ(args.size(), 1); \
    const auto affectedResources = nx::utils::toQSet(args[0].value<QnResourceList>()); \
    const auto expectedResources = nx::utils::toQSet( \
        std::initializer_list<QnResourcePtr>resources); \
    ASSERT_EQ(affectedResources, expectedResources); \
    permissionsDependencyChanged->clear(); }

TEST_F(PermissionDependenciesTest, resourceFlagsChanging)
{
    const auto layout = addLayout();
    CHECK_NO_DEPENDENCY_NOTIFICATIONS();
    layout->addFlags(Qn::remote);
    CHECK_DEPENDENCY_NOTIFICATION({layout});
}

TEST_F(PermissionDependenciesTest, layoutLockChanging)
{
    const auto layout = addLayout();
    CHECK_NO_DEPENDENCY_NOTIFICATIONS();
    layout->setLocked(true);
    CHECK_DEPENDENCY_NOTIFICATION({layout});
}

TEST_F(PermissionDependenciesTest, cameraCapabilitiesChanging)
{
    const auto camera = addCamera();
    CHECK_NO_DEPENDENCY_NOTIFICATIONS();
    camera->setCameraCapabilities(
        camera->getCameraCapabilities() | nx::vms::api::DeviceCapability::setUserPassword);
    CHECK_DEPENDENCY_NOTIFICATION({camera});
}

TEST_F(PermissionDependenciesTest, cameraLicenseTypeChanging)
{
    const auto camera = addCamera();
    CHECK_NO_DEPENDENCY_NOTIFICATIONS();
    camera->setProperty(ResourcePropertyKey::kForcedLicenseType,
        QString::fromStdString(nx::reflect::toString(Qn::LC_Bridge)));
    CHECK_DEPENDENCY_NOTIFICATION({camera});
}

TEST_F(PermissionDependenciesTest, cameraRecordingChanging)
{
    const auto camera = addCamera();
    CHECK_NO_DEPENDENCY_NOTIFICATIONS();
    camera->setScheduleEnabled(true);
    CHECK_DEPENDENCY_NOTIFICATION({camera});
}

TEST_F(PermissionDependenciesTest, userAttributesChanging)
{
    const auto user = addUser(NoGroup);
    CHECK_NO_DEPENDENCY_NOTIFICATIONS();
    user->setAttributes(api::UserAttribute::readonly);
    CHECK_DEPENDENCY_NOTIFICATION({user});
}

TEST_F(PermissionDependenciesTest, usersParentsChanging)
{
    auto group1 = createUserGroup("group1", api::kPowerUsersGroupId);
    const auto group2 = createUserGroup("group2", group1.id);
    const auto user1 = addUser(group1.id, "user1");
    const auto user2 = addUser(group2.id, "user2");
    CHECK_NO_DEPENDENCY_NOTIFICATIONS();

    group1.parentGroupIds = {};
    addOrUpdateUserGroup(group1);
    CHECK_DEPENDENCY_NOTIFICATION(({user1, user2}));
}

TEST_F(PermissionDependenciesTest, batchUpdate)
{
    const auto layout = addLayout();
    const auto camera = addCamera();
    const auto user = addUser(NoGroup);
    CHECK_NO_DEPENDENCY_NOTIFICATIONS();

    {
        const QnUpdatableGuard update(resourceAccessManager());
        layout->setLocked(true);
        camera->setScheduleEnabled(true);
        user->setAttributes(api::UserAttribute::readonly);
    }
    CHECK_DEPENDENCY_NOTIFICATION(({camera, layout, user}));
}

} // namespace nx::vms::common::test
