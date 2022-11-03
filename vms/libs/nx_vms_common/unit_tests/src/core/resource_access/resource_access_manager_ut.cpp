// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/core/access/access_types.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx_ec/data/api_conversion_functions.h>

namespace nx::vms::common::test {

using namespace nx::core::access;
using namespace nx::vms::api;

class ResourceAccessManagerTest: public ContextBasedTest
{
protected:
    virtual void TearDown() override
    {
        m_currentUser.clear();
    }

    QnLayoutResourcePtr createLayout(Qn::ResourceFlags flags, bool locked = false, const QnUuid &parentId = QnUuid())
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

    UserRoleData addAdministrators()
    {
        return createRole(GlobalPermission::adminPermissions);
    }

    UserRoleData addLiveViewers()
    {
        return createRole(GlobalPermission::liveViewerPermissions);
    }

    UserRoleData addViewers()
    {
        return createRole(GlobalPermission::viewerPermissions);
    }

    void setAccessRights(const QnUuid& subjectId, const ResourceAccessMap accessRights)
    {
        systemContext()->accessRightsManager()->setOwnResourceAccessMap(subjectId, accessRights);
    }

    void logout()
    {
        resourcePool()->removeResources(resourcePool()->getResourcesWithFlag(Qn::remote));
        m_currentUser.clear();
    }

    void loginAsOwner()
    {
        logout();
        auto user = createUser(GlobalPermission::none);
        user->setUserRoleIds({QnPredefinedUserRoles::id(Qn::UserRole::owner)});
        user->setOwner(true);
        resourcePool()->addResource(user);
        m_currentUser = user;
    }

    void loginAsCustom(GlobalPermissions globalPermissions)
    {
        logout();
        auto user = addUser(globalPermissions, QnResourcePoolTestHelper::kTestUserName);
        ASSERT_FALSE(user->isOwner());
        m_currentUser = user;
    }

    void loginAs(Qn::UserRole role)
    {
        const auto groupId = QnPredefinedUserRoles::id(role);
        ASSERT_FALSE(groupId.isNull());
        loginAsMember({groupId});
    }

    void loginAsMember(const QnUuid& groupId)
    {
        loginAsMember(std::vector<QnUuid>({groupId}));
    }

    void loginAsMember(const std::vector<QnUuid>& groupIds)
    {
        logout();
        auto user = addUser(GlobalPermissions{}, QnResourcePoolTestHelper::kTestUserName);
        user->setUserRoleIds(groupIds);
        ASSERT_FALSE(user->isOwner());
        m_currentUser = user;
    }

    bool hasPermission(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        Qn::Permissions requiredPermissions) const
    {
        return resourceAccessManager()->hasPermission(subject, resource, requiredPermissions);
    }

    void checkPermissions(
        const QnResourcePtr &resource, Qn::Permissions desired, Qn::Permissions forbidden) const
    {
        Qn::Permissions actual = resourceAccessManager()->permissions(m_currentUser, resource);
        ASSERT_EQ(desired, actual);
        ASSERT_EQ(0U, forbidden & actual);
    }

    void checkForbiddenPermissions(const QnResourcePtr &resource, Qn::Permissions forbidden) const
    {
        Qn::Permissions actual = resourceAccessManager()->permissions(m_currentUser, resource);
        ASSERT_EQ(0U, forbidden & actual);
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

    void checkCanGrantUserAdminPermissions(
        const QnUserResourcePtr& source, QnUserResourcePtr target, bool allowed)
    {
        UserData data;
        ec2::fromResourceToApi(target, data);
        data.permissions = GlobalPermission::admin;

        auto info = nx::format("check: %1 -> %2", source->getName(), target->getName())
            .toStdString();

        if (allowed)
            ASSERT_TRUE(resourceAccessManager()->canModifyUser(source, target, data)) << info;
        else
            ASSERT_FALSE(resourceAccessManager()->canModifyUser(source, target, data)) << info;
    }

    void checkCanGrantUserAdminPermissionsViaInheritance(const QnUserResourcePtr& source,
        QnUserResourcePtr target, const QnUuid& adminGroupId, bool allowed)
    {
        UserData data;
        ec2::fromResourceToApi(target, data);
        data.userRoleIds = {adminGroupId};

        auto info = nx::format("check: %1 -> %2", source->getName(), target->getName())
            .toStdString();

        if (allowed)
            ASSERT_TRUE(resourceAccessManager()->canModifyUser(source, target, data)) << info;
        else
            ASSERT_FALSE(resourceAccessManager()->canModifyUser(source, target, data)) << info;
    }

    QnUserResourcePtr m_currentUser;
};

/************************************************************************/
/* Checking layouts as admin                                            */
/************************************************************************/

/** Check permissions for common layout when the user is logged in as admin. */
TEST_F(ResourceAccessManagerTest, checkLayoutAsAdmin)
{
    const auto admins = addAdministrators();
    loginAsMember(admins.id);

    auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::NoPermissions;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked common layout when the user is logged in as admin. */
TEST_F(ResourceAccessManagerTest, checkLockedLayoutAsAdmin)
{
    const auto admins = addAdministrators();
    loginAsMember(admins.id);

    auto layout = createLayout(Qn::remote, true);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission
        | Qn::WritePermission
        | Qn::AddRemoveItemsPermission
        | Qn::WriteNamePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking own layouts as viewer                                       */
/************************************************************************/

/** Check permissions for common layout when the user is logged in as viewer. */
TEST_F(ResourceAccessManagerTest, checkLayoutAsViewer)
{
    const auto viewers = addLiveViewers();
    loginAsMember(viewers.id);

    auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::NoPermissions;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked common layout when the user is logged in as viewer. */
TEST_F(ResourceAccessManagerTest, checkLockedLayoutAsViewer)
{
    const auto viewers = addLiveViewers();
    loginAsMember(viewers.id);

    auto layout = createLayout(Qn::remote, true);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission
        | Qn::AddRemoveItemsPermission
        | Qn::WritePermission
        | Qn::WriteNamePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

TEST_F(ResourceAccessManagerTest, checkLockedChanged)
{
    loginAsCustom(GlobalPermission::none);
    auto user = m_currentUser;
    auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);
    ASSERT_TRUE(hasPermission(user, layout, Qn::AddRemoveItemsPermission));
    layout->setLocked(true);
    ASSERT_FALSE(hasPermission(user, layout, Qn::AddRemoveItemsPermission));
}

/************************************************************************/
/* Checking non-own remote layouts                                      */
/************************************************************************/
/** Check permissions for another viewer's layout when the user is logged in as viewer. */
TEST_F(ResourceAccessManagerTest, checkNonOwnViewersLayoutAsViewer)
{
    const auto viewers = addLiveViewers();
    loginAsMember(viewers.id);

    auto anotherUser = addUser(GlobalPermission::liveViewerPermissions,
        QnResourcePoolTestHelper::kTestUserName2);
    auto layout = createLayout(Qn::remote, false, anotherUser->getId());
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::NoPermissions;
    Qn::Permissions forbidden = Qn::FullLayoutPermissions;
    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for another viewer's layout when the user is logged in as admin. */
TEST_F(ResourceAccessManagerTest, checkNonOwnViewersLayoutAsAdmin)
{
    const auto admins = addAdministrators();
    loginAsMember(admins.id);

    auto anotherUser = addUser(GlobalPermission::liveViewerPermissions,
        QnResourcePoolTestHelper::kTestUserName2);
    auto layout = createLayout(Qn::remote, false, anotherUser->getId());
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::NoPermissions;
    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for another admin's layout when the user is logged in as admin. */
TEST_F(ResourceAccessManagerTest, checkNonOwnAdminsLayoutAsAdmin)
{
    const auto admins = addAdministrators();
    loginAsMember(admins.id);

    auto anotherUser = addUser(GlobalPermission::admin, QnResourcePoolTestHelper::kTestUserName2);
    auto layout = createLayout(Qn::remote, false, anotherUser->getId());
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::ModifyLayoutPermission;
    Qn::Permissions forbidden = Qn::FullLayoutPermissions;
    forbidden &= ~desired;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for another admin's layout when the user is logged in as owner. */
TEST_F(ResourceAccessManagerTest, checkNonOwnAdminsLayoutAsOwner)
{
    loginAsOwner();

    auto anotherUser = addUser(GlobalPermission::admin, QnResourcePoolTestHelper::kTestUserName2);
    auto layout = createLayout(Qn::remote, false, anotherUser->getId());
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::NoPermissions;
    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking shared layouts                                              */
/************************************************************************/
/** Check permissions for shared layout when the user is logged in as viewer. */
TEST_F(ResourceAccessManagerTest, checkSharedLayoutAsViewer)
{
    const auto viewers = addLiveViewers();
    loginAsMember(viewers.id);

    auto layout = createLayout(Qn::remote);
    layout->setParentId(QnUuid());
    resourcePool()->addResource(layout);

    ASSERT_TRUE(layout->isShared());

    /* By default user has no access to shared layouts. */
    Qn::Permissions desired = Qn::NoPermissions;
    Qn::Permissions forbidden = Qn::FullLayoutPermissions;
    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for shared layout when the user is logged in as admin. */
TEST_F(ResourceAccessManagerTest, checkSharedLayoutAsAdmin)
{
    const auto admins = addAdministrators();
    loginAsMember(admins.id);

    auto layout = createLayout(Qn::remote);
    layout->setParentId(QnUuid());
    resourcePool()->addResource(layout);

    ASSERT_TRUE(layout->isShared());

    /* Admin has full access to shared layouts. */
    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::NoPermissions;
    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for new shared layout when the user is logged in as admin. */
TEST_F(ResourceAccessManagerTest, checkNewSharedLayoutAsAdmin)
{
    const auto admins = addAdministrators();
    loginAsMember(admins.id);

    auto owner = createUser(GlobalPermission::admin);
    owner->setOwner(true);
    resourcePool()->addResource(owner);

    auto layout = createLayout(Qn::remote, false, owner->getId());
    resourcePool()->addResource(layout);

    /* Admin cannot modify owner's layout. */
    Qn::Permissions desired = Qn::ModifyLayoutPermission;
    Qn::Permissions forbidden = Qn::WriteNamePermission | Qn::SavePermission;
    checkPermissions(layout, desired, forbidden);

    layout->setParentId(QnUuid());

    /* Admin has full access to shared layouts. */
    checkPermissions(layout, Qn::FullLayoutPermissions, Qn::NoPermissions);
}

TEST_F(ResourceAccessManagerTest, checkParentChanged)
{
    loginAsCustom(GlobalPermission::none);
    auto user = m_currentUser;
    auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);
    layout->setParentId(QnUuid());
    ASSERT_FALSE(hasPermission(user, layout, Qn::ReadPermission));
}

//-------------------------------------------------------------------------------------------------
// Checking videowall-based layouts

/** Admin can do anything with layout on videowall. */
TEST_F(ResourceAccessManagerTest, checkVideowallLayoutAsAdmin)
{
    const auto admins = addAdministrators();
    loginAsMember(admins.id);

    auto videowall = addVideoWall();
    auto layout = createLayout(Qn::remote, false, videowall->getId());
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::NoPermissions;
    checkPermissions(layout, desired, forbidden);
}

/** Videowall-controller can do anything with layout on videowall. */
TEST_F(ResourceAccessManagerTest, checkVideowallLayoutAsVideowallUser)
{
    loginAsCustom(GlobalPermission::none);

    setAccessRights(m_currentUser->getId(),
        {{AccessRightsManager::kAnyResourceId, AccessRight::controlVideowall}});

    auto videowall = addVideoWall();
    auto layout = createLayout(Qn::remote, false, videowall->getId());
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::NoPermissions;
    checkPermissions(layout, desired, forbidden);
}

/** Viewer can't do anything with layout on videowall. */
TEST_F(ResourceAccessManagerTest, checkVideowallLayoutAsAdvancedViewer)
{
    loginAs(Qn::UserRole::advancedViewer);

    auto videowall = addVideoWall();
    auto layout = createLayout(Qn::remote, false, videowall->getId());
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::NoPermissions;
    Qn::Permissions forbidden = Qn::FullLayoutPermissions;
    checkPermissions(layout, desired, forbidden);
}

/** Locked layouts on videowall still can be removed if the user has control permissions. */
TEST_F(ResourceAccessManagerTest, checkVideowallLockedLayout)
{
    const auto group = createRole("Group", GlobalPermission::none);
    loginAsMember(group.id);

    setAccessRights(group.id,
        {{AccessRightsManager::kAnyResourceId, AccessRight::controlVideowall}});

    auto videowall = addVideoWall();
    auto layout = createLayout(Qn::remote, true, videowall->getId());
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::AddRemoveItemsPermission
        | Qn::WritePermission
        | Qn::WriteNamePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

TEST_F(ResourceAccessManagerTest, canAccessMyScreenOnVideoWallAsViewer)
{
    const auto group = addLiveViewers();
    loginAsMember(group.id);

    setAccessRights(group.id,
        {{AccessRightsManager::kAnyResourceId, AccessRight::controlVideowall}});

    auto camera = addDesktopCamera(m_currentUser);

    // User cannot see anybody's else screen.
    ASSERT_FALSE(hasPermission(m_currentUser, camera, Qn::ViewLivePermission));

    auto videowall = addVideoWall();
    auto layout = createLayout(Qn::remote, false, videowall->getId());

    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videowall->items()->addItem(vwitem);

    addToLayout(layout, camera);
    resourcePool()->addResource(layout);

    // Screen is available once it is added to videowall.
    ASSERT_TRUE(hasPermission(m_currentUser, camera, Qn::ViewLivePermission));
}

TEST_F(ResourceAccessManagerTest, cannotAccessAnyScreenAsOwner)
{
    loginAsOwner();
    auto camera = addDesktopCamera(m_currentUser);
    // User cannot see anybody's else screen, even his own.
    ASSERT_FALSE(hasPermission(m_currentUser, camera, Qn::ViewLivePermission));
}

// User can push it's own screen on a new videowall layout.
TEST_F(ResourceAccessManagerTest, canPushMyScreen)
{
    loginAsCustom(GlobalPermission::controlVideowall | GlobalPermission::viewerPermissions);

    auto videoWall = addVideoWall();
    auto ownScreen = addDesktopCamera(m_currentUser);

    QnLayoutResourcePtr layout = QnResourcePoolTestHelper::createLayout();
    layout->setParentId(videoWall->getId());
    addToLayout(layout, ownScreen);

    LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);

    ASSERT_TRUE(resourceAccessManager()->canCreateLayout(m_currentUser, layoutData));
}

// User can push it's own screen on exising videowall layout.
TEST_F(ResourceAccessManagerTest, canPushMyScreenOnExistingLayout)
{
    const auto group = addViewers();
    loginAsMember(group.id);

    setAccessRights(group.id,
        {{AccessRightsManager::kAnyResourceId, AccessRight::controlVideowall}});

    auto videoWall = addVideoWall();
    auto ownScreen = addDesktopCamera(m_currentUser);
    auto layout = addLayout();
    layout->setParentId(videoWall->getId());

    LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);

    LayoutItemData item;
    item.id = QnUuid::createUuid();
    item.resourceId = ownScreen->getId();
    layoutData.items.push_back(item);

    ASSERT_TRUE(resourceAccessManager()->canModifyResource(m_currentUser, layout, layoutData));
}

// Pushing screen is allowed only if layout belongs to videowall.
TEST_F(ResourceAccessManagerTest, cannotPushMyScreenNotOnVideoWall)
{
    loginAsOwner();

    auto ownScreen = addDesktopCamera(m_currentUser);
    auto layout = QnResourcePoolTestHelper::createLayout();
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
    loginAsOwner();

    auto ownScreen = addDesktopCamera(m_currentUser);
    auto layout = addLayout();

    LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);

    LayoutItemData item;
    item.id = QnUuid::createUuid();
    item.resourceId = ownScreen->getId();
    layoutData.items.push_back(item);

    ASSERT_FALSE(resourceAccessManager()->canModifyResource(m_currentUser, layout, layoutData));
}

// Pushing screen is allowed only if there are video wall permissions.
TEST_F(ResourceAccessManagerTest, viewerCannotPushOwnScreen)
{
    const auto group = addViewers();
    loginAsMember(group.id);

    auto videoWall = addVideoWall();
    auto ownScreen = addDesktopCamera(m_currentUser);

    auto layout = QnResourcePoolTestHelper::createLayout();
    layout->setParentId(videoWall->getId());
    addToLayout(layout, ownScreen);

    LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);

    ASSERT_FALSE(resourceAccessManager()->canCreateLayout(m_currentUser, layoutData));
}

// Pushing screen is allowed only if there are video wall permissions.
TEST_F(ResourceAccessManagerTest, viewerCannotPushOwnScreenOnExistingLayout)
{
    const auto group = addViewers();
    loginAsMember(group.id);

    auto videoWall = addVideoWall();
    auto ownScreen = addDesktopCamera(m_currentUser);
    auto layout = addLayout();
    layout->setParentId(videoWall->getId());

    LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);

    LayoutItemData item;
    item.id = QnUuid::createUuid();
    item.resourceId = ownScreen->getId();
    layoutData.items.push_back(item);

    ASSERT_FALSE(resourceAccessManager()->canModifyResource(m_currentUser, layout, layoutData));
}

// Pushing screen is allowed only for own screen.
TEST_F(ResourceAccessManagerTest, cannotPushOtherUsersScreen)
{
    loginAsOwner();

    auto anotherUser = addUser(
        GlobalPermission::controlVideowall | GlobalPermission::viewerPermissions);
    auto videoWall = addVideoWall();
    auto otherScreen = addDesktopCamera(anotherUser);

    auto layout = QnResourcePoolTestHelper::createLayout();
    layout->setParentId(videoWall->getId());
    addToLayout(layout, otherScreen);

    LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);

    ASSERT_FALSE(resourceAccessManager()->canCreateLayout(m_currentUser, layoutData));
}

// Pushing screen is allowed only for own screen.
TEST_F(ResourceAccessManagerTest, cannotPushOtherUsersScreenOnExistingLayout)
{
    loginAsOwner();

    auto anotherUser = addUser(
        GlobalPermission::controlVideowall | GlobalPermission::viewerPermissions);
    auto videoWall = addVideoWall();
    auto otherScreen = addDesktopCamera(anotherUser);
    auto layout = addLayout();
    layout->setParentId(videoWall->getId());

    LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);

    LayoutItemData item;
    item.id = QnUuid::createUuid();
    item.resourceId = otherScreen->getId();
    layoutData.items.push_back(item);

    ASSERT_FALSE(resourceAccessManager()->canModifyResource(m_currentUser, layout, layoutData));
}

// Pushing screen is allowed only if it is the only camera on layout.
TEST_F(ResourceAccessManagerTest, cannotPushScreenWithCameras)
{
    loginAsOwner();

    auto videoWall = addVideoWall();
    auto ownScreen = addDesktopCamera(m_currentUser);
    auto camera = addCamera();

    auto layout = QnResourcePoolTestHelper::createLayout();
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
    loginAsOwner();

    auto videoWall = addVideoWall();
    auto ownScreen = addDesktopCamera(m_currentUser);
    auto camera = addCamera();
    auto layout = addLayout();
    layout->setParentId(videoWall->getId());

    LayoutData layoutData;
    ec2::fromResourceToApi(layout, layoutData);

    {
        LayoutItemData item;
        item.id = QnUuid::createUuid();
        item.resourceId = ownScreen->getId();
        layoutData.items.push_back(item);
    }
    {
        LayoutItemData item;
        item.id = QnUuid::createUuid();
        item.resourceId = camera->getId();
        layoutData.items.push_back(item);
    }

    ASSERT_FALSE(resourceAccessManager()->canModifyResource(m_currentUser, layout, layoutData));
}

/************************************************************************/
/* Checking user access rights                                          */
/************************************************************************/

/** Check user can edit himself (but cannot rename, remove and change access rights). */
TEST_F(ResourceAccessManagerTest, checkUserEditHimself)
{
    loginAsOwner();

    Qn::Permissions desired = Qn::FullUserPermissions;
    Qn::Permissions forbidden = Qn::WriteNamePermission | Qn::RemovePermission | Qn::WriteAccessRightsPermission;
    desired &= ~forbidden;

    checkPermissions(m_currentUser, desired, forbidden);
}

TEST_F(ResourceAccessManagerTest, checkEditDisabledAdmin)
{
    const auto admins = addAdministrators();
    loginAsMember(admins.id);
    auto otherAdmin = createUser(GlobalPermission::admin);
    otherAdmin->setEnabled(false);
    resourcePool()->addResource(otherAdmin);
    ASSERT_FALSE(hasPermission(m_currentUser, otherAdmin, Qn::WriteAccessRightsPermission));
}

TEST_F(ResourceAccessManagerTest, checkEditDisabledOwner)
{
    const auto localOwner = createUser(
        GlobalPermission::none, "admin", UserType::local);
    localOwner->setOwner(true);
    localOwner->setEnabled(false);
    resourcePool()->addResource(localOwner);

    const auto cloudOwner = createUser(
        GlobalPermission::none, "user@mail.com", UserType::cloud);
    cloudOwner->setOwner(true);
    resourcePool()->addResource(cloudOwner);

    ASSERT_FALSE(hasPermission(cloudOwner, localOwner, Qn::WriteAccessRightsPermission));

    UserData data;
    ec2::fromResourceToApi(localOwner, data);
    data.isEnabled = true;
    ASSERT_FALSE(resourceAccessManager()->canModifyUser(cloudOwner, localOwner, data));
}

TEST_F(ResourceAccessManagerTest, checkOwnerCanNotEditOtherOwner)
{
    const auto localOwner = createUser(
        GlobalPermission::none, "admin", UserType::local);
    localOwner->setOwner(true);
    resourcePool()->addResource(localOwner);

    const auto cloudOwner = createUser(
        GlobalPermission::none, "user@mail.com", UserType::cloud);
    cloudOwner->setOwner(true);
    resourcePool()->addResource(cloudOwner);

    EXPECT_FALSE(hasPermission(localOwner, cloudOwner, Qn::WriteDigestPermission));
    EXPECT_TRUE(hasPermission(cloudOwner, localOwner, Qn::WriteDigestPermission));

    for (const auto& permission: {
        Qn::WritePasswordPermission, Qn::WriteAccessRightsPermission,
        Qn::WriteEmailPermission, Qn::WriteFullNamePermission,
    })
    {
        const auto label = QJson::serialized(permission).toStdString();
        EXPECT_FALSE(hasPermission(localOwner, cloudOwner, permission)) << label;
        EXPECT_FALSE(hasPermission(cloudOwner, localOwner, permission)) << label;
    }
}

TEST_F(ResourceAccessManagerTest, checkCanModifyUserDigestAuthorizationEnabledHimself)
{
    loginAs(Qn::UserRole::liveViewer);
    checkCanModifyUserDigestAuthorizationEnabled(m_currentUser, /*allowed*/ false);

    const auto viewers = addLiveViewers();
    loginAsMember(viewers.id);
    checkCanModifyUserDigestAuthorizationEnabled(m_currentUser, /*allowed*/ false);

    loginAs(Qn::UserRole::administrator);
    checkCanModifyUserDigestAuthorizationEnabled(m_currentUser, /*allowed*/ false);

    const auto admins = addAdministrators();
    loginAsMember(admins.id);
    checkCanModifyUserDigestAuthorizationEnabled(m_currentUser, /*allowed*/ false);

    loginAsOwner();
    checkCanModifyUserDigestAuthorizationEnabled(m_currentUser, /*allowed*/ true);

    UserData data;
    ec2::fromResourceToApi(m_currentUser, data);
    data.digest = invertedDigestAuthorizationEnabled(m_currentUser);
    data.name += "_changed";
    ASSERT_FALSE(resourceAccessManager()->canModifyUser(m_currentUser, m_currentUser, data));
}

TEST_F(ResourceAccessManagerTest, checkCanModifyUserDigestAuthorizationEnabledAdmin)
{
    const auto admins = addAdministrators();
    auto admin = createUser(GlobalPermission::none);
    admin->setUserRoleIds({admins.id});
    admin->removeFlags(Qn::remote); //< Prevent removing of this user by loginAs();
    resourcePool()->addResource(admin);

    loginAs(Qn::UserRole::liveViewer);
    checkCanModifyUserDigestAuthorizationEnabled(admin, /*allowed*/ true);

    const auto viewers = addLiveViewers();
    loginAsMember(viewers.id);
    checkCanModifyUserDigestAuthorizationEnabled(admin, /*allowed*/ true);

    loginAs(Qn::UserRole::administrator);
    checkCanModifyUserDigestAuthorizationEnabled(admin, /*allowed*/ false);

    loginAsMember(admins.id);
    checkCanModifyUserDigestAuthorizationEnabled(admin, /*allowed*/ false);

    loginAsOwner();
    checkCanModifyUserDigestAuthorizationEnabled(admin, /*allowed*/ false);
}

TEST_F(ResourceAccessManagerTest, checkCanModifyUserDigestAuthorizationEnabled)
{
    auto owner = addUser(GlobalPermission::none, "owner");
    owner->setOwner(true);
    auto cloud = addUser(GlobalPermission::none, "cloud", UserType::cloud);
    auto cloudOwner =
        addUser(GlobalPermission::none, "cloud owner", UserType::cloud);
    cloudOwner->setOwner(true);
    auto admin = addUser(GlobalPermission::admin, "admin");
    auto ldap = addUser(GlobalPermission::none, "ldap", UserType::ldap, "cn=ldap");
    auto other = addUser(GlobalPermission::none, "other");

    QnUserResourceList users = {owner, cloudOwner, admin, cloud, ldap, other};
    QVector<std::pair<QnUserResourcePtr, QnUserResourcePtr>> allowedScenarios = {
        {owner, owner},
        {owner, admin},
        {owner, ldap},
        {owner, other},
        {cloudOwner, owner},
        {cloudOwner, admin},
        {cloudOwner, ldap},
        {cloudOwner, other},
        {admin, other},
        {admin, ldap},
    };

    for (const auto source: users)
    {
        for (const auto target: users)
        {
            const bool allowed = allowedScenarios.contains({source, target});
            checkCanModifyUserDigestAuthorizationEnabled(source, target, allowed);
        }
    }
}

TEST_F(ResourceAccessManagerTest, checkCanGrantUserAdminPermissions)
{
    const auto admins = addAdministrators();
    const auto adminsToo = addAdministrators();
    const auto owner = addUser(GlobalPermission::none, "owner");
    owner->setOwner(true);
    const auto cloud = addUser(GlobalPermission::none, "cloud", UserType::cloud);
    const auto cloudOwner =
        addUser(GlobalPermission::none, "cloud owner", UserType::cloud);
    cloudOwner->setOwner(true);
    const auto admin = addUser(GlobalPermission::admin, "admin");
    const auto inheritedAdmin = addUser(GlobalPermission::none, "inherited_admin");
    inheritedAdmin->setUserRoleIds({adminsToo.id});
    const auto ldap = addUser(GlobalPermission::none, "ldap", UserType::ldap, "cn=ldap");
    const auto other = addUser(GlobalPermission::none, "other");

    QnUserResourceList users = {owner, cloudOwner, admin, inheritedAdmin, cloud, ldap, other};
    QVector<std::pair<QnUserResourcePtr, QnUserResourcePtr>> allowedScenarios = {
        {owner, admin},
        {owner, inheritedAdmin},
        {owner, ldap},
        {owner, other},
        {owner, cloud},
        {cloudOwner, admin},
        {cloudOwner, inheritedAdmin},
        {cloudOwner, cloud},
        {cloudOwner, ldap},
        {cloudOwner, other}
    };

    for (const auto source: users)
    {
        for (const auto target: users)
        {
            const bool allowed = allowedScenarios.contains({source, target});
            checkCanGrantUserAdminPermissions(source, target, allowed);
            checkCanGrantUserAdminPermissionsViaInheritance(source, target, admins.id, allowed);
        }
    }
}

TEST_F(ResourceAccessManagerTest, checkWhatUsersCanViewerCreate)
{
    const auto admins = addAdministrators();
    const auto viewers = addViewers();
    loginAsMember(viewers.id);

    // Cannot create viewers.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::liveViewerPermissions, {}, false));

    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {viewers.id}, false));

    // Cannot create admins.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::admin, {}, false));

    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {admins.id}, false));

    // Cannot create owners.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::admin, {}, true));
}

TEST_F(ResourceAccessManagerTest, checkWhatUsersCanAdminCreate)
{
    const auto admins = addAdministrators();
    const auto viewers = addViewers();
    loginAsMember(admins.id);

    // Can create viewers.
    ASSERT_TRUE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::liveViewerPermissions, {}, false));

    ASSERT_TRUE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {viewers.id}, false));

    // Cannot create admins.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::admin, {}, false));

    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {admins.id}, false));

    // Cannot create owners.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::admin, {}, true));
}

TEST_F(ResourceAccessManagerTest, checkWhatUsersCanOwnerCreate)
{
    const auto admins = addAdministrators();
    const auto viewers = addViewers();

    loginAsOwner();

    // Can create viewers.
    ASSERT_TRUE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::liveViewerPermissions, {}, false));

    ASSERT_TRUE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {viewers.id}, false));

    // Can create admins.
    ASSERT_TRUE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::admin, {}, false));

    ASSERT_TRUE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {admins.id}, false));

    // Cannot create owners.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::admin, {}, true));
}

TEST_F(ResourceAccessManagerTest, checkParentGroupsValidity)
{
    loginAsOwner();

    const auto predefinedId = QnPredefinedUserRoles::id(Qn::UserRole::viewer);
    const auto customId = createRole(GlobalPermission::none,
        {QnPredefinedUserRoles::id(Qn::UserRole::advancedViewer)}).id;

    const auto zeroId = QnUuid{};
    const auto unknownId = QnUuid::createUuid();

    ASSERT_TRUE(resourceAccessManager()->canCreateUser(m_currentUser, GlobalPermission::none,
        {predefinedId, customId}, /*isOwner*/ false));

    ASSERT_FALSE(resourceAccessManager()->canCreateUser(m_currentUser, GlobalPermission::none,
        {predefinedId, zeroId, customId}, /*isOwner*/ false));

    ASSERT_FALSE(resourceAccessManager()->canCreateUser(m_currentUser, GlobalPermission::none,
        {predefinedId, unknownId, customId}, /*isOwner*/ false));

    const auto user = addUser(GlobalPermission::none);
    UserData userData;
    ec2::fromResourceToApi(user, userData);

    userData.userRoleIds = {predefinedId, customId};
    ASSERT_TRUE(resourceAccessManager()->canModifyUser(m_currentUser, user, userData));

    userData.userRoleIds = {predefinedId, zeroId, customId};
    ASSERT_FALSE(resourceAccessManager()->canModifyUser(m_currentUser, user, userData));

    userData.userRoleIds = {predefinedId, unknownId, customId};
    ASSERT_FALSE(resourceAccessManager()->canModifyUser(m_currentUser, user, userData));
}

/************************************************************************/
/* Checking cameras access rights                                       */
/************************************************************************/

// Nobody can view desktop camera footage.
TEST_F(ResourceAccessManagerTest, checkDesktopCameraFootage)
{
    loginAsOwner();

    auto camera = createCamera();
    camera->addFlags(Qn::desktop_camera);
    resourcePool()->addResource(camera);

    ASSERT_FALSE(hasPermission(m_currentUser, camera,
        Qn::ViewFootagePermission));
}

/** Check owner can remove non-owned desktop camera, but cannot view it. */
TEST_F(ResourceAccessManagerTest, checkDesktopCameraRemove)
{
    loginAsOwner();

    auto camera = createCamera();
    camera->addFlags(Qn::desktop_camera);
    resourcePool()->addResource(camera);

    Qn::Permissions desired = Qn::RemovePermission;
    Qn::Permissions forbidden = Qn::ReadPermission;
    desired &= ~forbidden;

    checkPermissions(camera, desired, forbidden);
}

TEST_F(ResourceAccessManagerTest, checkRemoveCameraAsAdmin)
{
    const auto admins = addAdministrators();
    loginAsMember(admins.id);

    const auto target = addCamera();
    ASSERT_TRUE(hasPermission(m_currentUser, target, Qn::RemovePermission));
}

// EditCameras is not enough to be able to remove cameras
TEST_F(ResourceAccessManagerTest, checkRemoveCameraAsEditor)
{
    auto user = addUser(GlobalPermission::none);
    auto target = addCamera();

    setAccessRights(user->getId(), {{target->getId(),
        AccessRight::view | AccessRight::edit}});

    ASSERT_TRUE(hasPermission(user, target, Qn::WritePermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::SavePermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::RemovePermission));
}

TEST_F(ResourceAccessManagerTest, checkViewCameraPermission)
{
    auto admin = addUser(GlobalPermission::admin);
    auto advancedViewer = addUser(GlobalPermission::advancedViewerPermissions);
    auto viewer = addUser(GlobalPermission::viewerPermissions);
    auto live = addUser(GlobalPermission::liveViewerPermissions);
    auto target = addCamera();

    auto viewLivePermission = Qn::ReadPermission
        | Qn::ViewContentPermission
        | Qn::ViewLivePermission;
    auto viewPermission = viewLivePermission | Qn::ViewFootagePermission;

    ASSERT_TRUE(hasPermission(admin, target, viewPermission));
    ASSERT_TRUE(hasPermission(advancedViewer, target, viewPermission));
    ASSERT_TRUE(hasPermission(viewer, target, viewPermission));
    ASSERT_TRUE(hasPermission(live, target, viewLivePermission));
    ASSERT_FALSE(hasPermission(live, target, Qn::ViewFootagePermission));
}

TEST_F(ResourceAccessManagerTest, checkExportCameraPermission)
{
    auto admin = addUser(GlobalPermission::admin);
    auto advancedViewer = addUser(GlobalPermission::advancedViewerPermissions);
    auto viewer = addUser(GlobalPermission::viewerPermissions);
    auto live = addUser(GlobalPermission::liveViewerPermissions);
    auto target = addCamera();

    auto exportPermission = Qn::ReadPermission
        | Qn::ViewContentPermission
        | Qn::ViewFootagePermission
        | Qn::ExportPermission;

    ASSERT_TRUE(hasPermission(admin, target, exportPermission));
    ASSERT_TRUE(hasPermission(advancedViewer, target, exportPermission));
    ASSERT_TRUE(hasPermission(viewer, target, exportPermission));
    ASSERT_FALSE(hasPermission(live, target, exportPermission));
}

TEST_F(ResourceAccessManagerTest, checkUserRemoved)
{
    auto user = addUser(GlobalPermission::admin);
    auto camera = addCamera();

    ASSERT_TRUE(hasPermission(user, camera, Qn::RemovePermission));
    resourcePool()->removeResource(user);
    ASSERT_FALSE(hasPermission(user, camera, Qn::RemovePermission));
}

TEST_F(ResourceAccessManagerTest, checkUserRoleChange)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::none);
    auto group = createRole(GlobalPermission::accessAllMedia);
    user->setUserRoleIds({group.id});

    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));
}

TEST_F(ResourceAccessManagerTest, checkUserEnabledChange)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));

    user->setEnabled(false);
    ASSERT_FALSE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::ViewContentPermission));
}

TEST_F(ResourceAccessManagerTest, checkRoleAccessChange)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::none);
    auto role = createRole(GlobalPermission::none);

    user->setUserRoleIds({role.id});
    ASSERT_FALSE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::ViewContentPermission));

    role.permissions = GlobalPermission::accessAllMedia;
    userRolesManager()->addOrUpdateUserRole(role);

    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));
}

TEST_F(ResourceAccessManagerTest, checkInheritedRoleAccessChange)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::none);
    auto parentRole = createRole(GlobalPermission::none);
    auto inheritedRole = createRole(GlobalPermission::none, {parentRole.id});

    user->setUserRoleIds({inheritedRole.id});
    ASSERT_FALSE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::ViewContentPermission));

    parentRole.permissions = GlobalPermission::accessAllMedia;
    userRolesManager()->addOrUpdateUserRole(parentRole);
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));

    inheritedRole.parentRoleIds = {};
    userRolesManager()->addOrUpdateUserRole(inheritedRole);
    ASSERT_FALSE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::ViewContentPermission));

    user->setUserRoleIds({inheritedRole.id, parentRole.id});
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));

    userRolesManager()->removeUserRole(parentRole.id);
    ASSERT_FALSE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::ViewContentPermission));
}

TEST_F(ResourceAccessManagerTest, checkUserAndRolesCombinedPermissions)
{
    auto cameraOfParentRole1 = addCamera();
    auto cameraOfParentRole2 = addCamera();
    auto cameraOfInheridedRole = addCamera();
    auto cameraOfUser = addCamera();
    auto cameraOfNoOne = addCamera();

    auto parentRole1 = createRole(GlobalPermission::none);
    sharedResourcesManager()->setSharedResources(parentRole1, {cameraOfParentRole1->getId()});
    EXPECT_FALSE(hasPermission(parentRole1, cameraOfUser, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(parentRole1, cameraOfParentRole1, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(parentRole1, cameraOfParentRole2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(parentRole1, cameraOfInheridedRole, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(parentRole1, cameraOfNoOne, Qn::ReadPermission));

    auto parentRole2 = createRole(GlobalPermission::none);
    sharedResourcesManager()->setSharedResources(parentRole2, {cameraOfParentRole2->getId()});
    EXPECT_FALSE(hasPermission(parentRole2, cameraOfUser, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(parentRole2, cameraOfParentRole1, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(parentRole2, cameraOfParentRole2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(parentRole2, cameraOfInheridedRole, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(parentRole2, cameraOfNoOne, Qn::ReadPermission));

    auto inheritedRole = createRole(GlobalPermission::none, {parentRole1.id, parentRole2.id});
    sharedResourcesManager()->setSharedResources(inheritedRole, {cameraOfInheridedRole->getId()});
    EXPECT_FALSE(hasPermission(inheritedRole, cameraOfUser, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(inheritedRole, cameraOfParentRole1, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(inheritedRole, cameraOfParentRole2, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(inheritedRole, cameraOfInheridedRole, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(inheritedRole, cameraOfNoOne, Qn::ReadPermission));

    auto user = addUser(GlobalPermission::none, "vasily");
    EXPECT_FALSE(hasPermission(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfParentRole1, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfParentRole2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfInheridedRole, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfNoOne, Qn::ReadPermission));

    sharedResourcesManager()->setSharedResources(user, {cameraOfUser->getId()});
    EXPECT_TRUE(hasPermission(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfParentRole1, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfParentRole2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfInheridedRole, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfNoOne, Qn::ReadPermission));

    user->setUserRoleIds({parentRole1.id});
    EXPECT_TRUE(hasPermission(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(user, cameraOfParentRole1, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfParentRole2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfInheridedRole, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfNoOne, Qn::ReadPermission));

    user->setUserRoleIds({parentRole1.id, parentRole2.id});
    EXPECT_TRUE(hasPermission(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(user, cameraOfParentRole1, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(user, cameraOfParentRole2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfInheridedRole, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfNoOne, Qn::ReadPermission));

    user->setUserRoleIds({inheritedRole.id});
    EXPECT_TRUE(hasPermission(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(user, cameraOfParentRole1, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(user, cameraOfParentRole2, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(user, cameraOfInheridedRole, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfNoOne, Qn::ReadPermission));

    inheritedRole.parentRoleIds = {};
    userRolesManager()->addOrUpdateUserRole(inheritedRole);
    EXPECT_TRUE(hasPermission(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfParentRole1, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfParentRole2, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(user, cameraOfInheridedRole, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfNoOne, Qn::ReadPermission));

    user->setUserRoleIds({});
    EXPECT_TRUE(hasPermission(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfParentRole1, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfParentRole2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfInheridedRole, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfNoOne, Qn::ReadPermission));
}

TEST_F(ResourceAccessManagerTest, checkEditAccessChange)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::accessAllMedia);
    ASSERT_FALSE(hasPermission(user, target, Qn::SavePermission));

    user->setRawPermissions(GlobalPermission::accessAllMedia | GlobalPermission::editCameras);
    ASSERT_TRUE(hasPermission(user, target, Qn::SavePermission));
}

TEST_F(ResourceAccessManagerTest, checkRoleRemoved)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::none);
    auto role = createRole(GlobalPermission::accessAllMedia);

    user->setUserRoleIds({role.id});
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));

    userRolesManager()->removeUserRole(role.id);
    ASSERT_FALSE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::ViewContentPermission));
}

TEST_F(ResourceAccessManagerTest, checkCameraOnVideoWall)
{
    loginAs(Qn::UserRole::administrator);
    auto target = addCamera();
    auto videoWall = addVideoWall();
    auto user = addUser(GlobalPermission::controlVideowall);
    auto layout = createLayout(Qn::remote, false, videoWall->getId());
    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videoWall->items()->addItem(vwitem);

    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);
    resourcePool()->addResource(layout);
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));
}

TEST_F(ResourceAccessManagerTest, checkShareLayoutToRole)
{
    loginAs(Qn::UserRole::administrator);

    auto target = addCamera();

    // Create role without access
    auto role = createRole(GlobalPermission::none);

    // Create user in role
    auto user = addUser(GlobalPermission::none, kTestUserName2);
    user->setUserRoleIds({role.id});

    // Create own layout
    auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);

    // Place a camera on it
    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    // Share layout to _role_
    layout->setParentId(QnUuid());
    sharedResourcesManager()->setSharedResources(role, {layout->getId()});

    // Make sure user got permissions
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
}

TEST_F(ResourceAccessManagerTest, checkShareLayoutToParentRole)
{
    loginAs(Qn::UserRole::administrator);

    auto target = addCamera();

    // Create roles without access
    auto parentRole = createRole(GlobalPermission::none);
    auto inheritedRole = createRole(GlobalPermission::none, {parentRole.id});

    // Create user in role
    auto user = addUser(GlobalPermission::none, kTestUserName2);
    user->setUserRoleIds({inheritedRole.id});

    // Create own layout
    auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);

    // Place a camera on it
    QnLayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    // Share layout to _role_
    layout->setParentId(QnUuid());
    sharedResourcesManager()->setSharedResources(parentRole, {layout->getId()});

    // Make sure user got permissions
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
}

/**
 * VMAXes without licences:
 * Viewing live: temporary allowed
 * Viewing footage: temporary allowed
 * Export video: temporary allowed
 */
TEST_F(ResourceAccessManagerTest, checkVMaxWithoutLicense)
{
    auto user = addUser(GlobalPermission::admin);
    auto camera = addCamera();

    // Camera is detected as VMax
    camera->markCameraAsVMax();
    ASSERT_TRUE(camera->licenseType() == Qn::LC_VMAX);
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewContentPermission));

    // TODO: Forbid all for VMAX when discussed with management
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewLivePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewFootagePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ExportPermission));

    // License enabled
    camera->setScheduleEnabled(true);
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewContentPermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewLivePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewFootagePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ExportPermission));
}

/**
 * NVRs without licences:
 * Viewing live: allowed
 * Viewing footage: forbidden
 * Export video: forbidden
 */
TEST_F(ResourceAccessManagerTest, checkNvrWithoutLicense)
{
    auto user = addUser(GlobalPermission::admin);
    auto camera = addCamera();

    // Camera is detected as NVR
    camera->markCameraAsNvr();
    ASSERT_TRUE(camera->licenseType() == Qn::LC_Bridge);
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewContentPermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewLivePermission));
    ASSERT_FALSE(hasPermission(user, camera, Qn::ViewFootagePermission));
    ASSERT_FALSE(hasPermission(user, camera, Qn::ExportPermission));

    // License enabled
    camera->setScheduleEnabled(true);
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewContentPermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewLivePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewFootagePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ExportPermission));
}

/**
* Camera with default auth (if can be changed) must not be viewable.
*/
TEST_F(ResourceAccessManagerTest, checkDefaultAuthCamera)
{
    auto user = addUser(GlobalPermission::admin);
    auto camera = addCamera();

    camera->setCameraCapabilities(
        DeviceCapabilities(DeviceCapability::setUserPassword)
        | DeviceCapability::isDefaultPassword);

    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewContentPermission));
    ASSERT_FALSE(hasPermission(user, camera, Qn::ViewLivePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewFootagePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ExportPermission));

    // Password changed
    camera->setCameraCapabilities(DeviceCapability::setUserPassword);
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewContentPermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewLivePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewFootagePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ExportPermission));
}

/**
* Camera with default auth must be viewable if password cannot be changed.
*/
TEST_F(ResourceAccessManagerTest, checkDefaultAuthCameraNonChangeable)
{
    auto user = addUser(GlobalPermission::admin);
    auto camera = addCamera();

    camera->setCameraCapabilities(DeviceCapability::isDefaultPassword);
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewContentPermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewLivePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewFootagePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ExportPermission));
}

/************************************************************************/
/* Checking servers access rights                                       */
/************************************************************************/

/* Admin must have full permissions for servers. */
TEST_F(ResourceAccessManagerTest, checkServerAsAdmin)
{
    loginAsOwner();

    auto server = addServer();

    Qn::Permissions desired = Qn::FullServerPermissions;
    Qn::Permissions forbidden = Qn::NoPermissions;

    checkPermissions(server, desired, forbidden);
}

/* Custom users can't view health monitors by default. */
TEST_F(ResourceAccessManagerTest, checkServerAsCustom)
{
    loginAsCustom(GlobalPermission::customUser);

    auto server = addServer();

    Qn::Permissions desired = Qn::ReadPermission;
    Qn::Permissions forbidden = Qn::FullServerPermissions;
    forbidden &= ~desired;

    checkPermissions(server, desired, forbidden);
}

/* User with live viewer permissions can view health monitors by default. */
TEST_F(ResourceAccessManagerTest, checkServerAsLiveViewer)
{
    loginAs(Qn::UserRole::liveViewer);

    auto server = addServer();

    Qn::Permissions desired = Qn::ReadPermission | Qn::ViewContentPermission;
    Qn::Permissions forbidden = Qn::FullServerPermissions;
    forbidden &= ~desired;

    checkPermissions(server, desired, forbidden);
}

/************************************************************************/
/* Checking storages access rights                                       */
/************************************************************************/

/* Admin must have full permissions for storages. */
TEST_F(ResourceAccessManagerTest, checkStoragesAsAdmin)
{
    loginAsOwner();

    auto server = addServer();
    auto storage = addStorage(server);

    Qn::Permissions desired = Qn::ReadWriteSavePermission | Qn::RemovePermission;
    Qn::Permissions forbidden = Qn::NoPermissions;

    checkPermissions(storage, desired, forbidden);
}

/* Non-admin users should not have access to storages. */
TEST_F(ResourceAccessManagerTest, checkStoragesAsCustom)
{
    loginAsCustom(GlobalPermission::customUser);

    auto server = addServer();
    auto storage = addStorage(server);

    Qn::Permissions desired = Qn::NoPermissions;
    Qn::Permissions forbidden = Qn::ReadWriteSavePermission | Qn::RemovePermission;

    /* No access to server storages for custom user. */
    checkPermissions(storage, desired, forbidden);
}

/* Non-admin users should not have access to storages. */
TEST_F(ResourceAccessManagerTest, checkStoragesAsLiveViewer)
{
    loginAs(Qn::UserRole::liveViewer);

    auto server = addServer();
    auto storage = addStorage(server);

    Qn::Permissions desired = Qn::NoPermissions;
    Qn::Permissions forbidden = Qn::ReadWriteSavePermission | Qn::RemovePermission;

    /* We do have access to server, but no access to its storages. */
    checkPermissions(storage, desired, forbidden);
}

/************************************************************************/
/* Checking videowall access rights                                     */
/************************************************************************/

/* Admin must have full permissions for videowalls. */
TEST_F(ResourceAccessManagerTest, checkVideowallAsAdmin)
{
    loginAsOwner();

    auto videowall = addVideoWall();

    Qn::Permissions desired = Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;
    Qn::Permissions forbidden = Qn::NoPermissions;

    checkPermissions(videowall, desired, forbidden);
}

/* Videowall control user must have almost full permissions for videowalls. */
TEST_F(ResourceAccessManagerTest, checkVideowallAsController)
{
    loginAsCustom(GlobalPermission::customUser | GlobalPermission::controlVideowall);

    auto videowall = addVideoWall();

    Qn::Permissions desired = Qn::ReadWriteSavePermission | Qn::WriteNamePermission;
    Qn::Permissions forbidden = Qn::RemovePermission;

    checkPermissions(videowall, desired, forbidden);
}

/* Videowall is inaccessible for default user. */
TEST_F(ResourceAccessManagerTest, checkVideowallAsViewer)
{
    loginAs(Qn::UserRole::advancedViewer);

    auto videowall = addVideoWall();

    Qn::Permissions desired = Qn::NoPermissions;
    Qn::Permissions forbidden = Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    checkPermissions(videowall, desired, forbidden);
}

} // namespace nx::vms::common::test
