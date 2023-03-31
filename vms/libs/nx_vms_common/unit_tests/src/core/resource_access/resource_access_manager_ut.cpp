// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/core/access/access_types.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/api/data/access_rights_data.h>
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

    void setOwnAccessRights(const QnUuid& subjectId, const ResourceAccessMap accessRights)
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
        loginAs(kOwnersGroupId);
    }

    void loginAsCustom()
    {
        loginAs(NoGroup);
    }

    void loginAs(Ids groupIds)
    {
        logout();
        auto user = addUser(groupIds);
        ASSERT_EQ(user->isOwner(), (groupIds.data == std::vector<QnUuid>{kOwnersGroupId}));
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
        data.permissions = target->getRawPermissions() | GlobalPermission::admin;

        auto info = nx::format("check: %1 -> %2 (direct)", source->getName(), target->getName())
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
        data.groupIds = target->groupIds();
        data.groupIds.push_back(adminGroupId);

        auto info = nx::format("check: %1 -> %2 (inheritance)", source->getName(), target->getName())
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
    loginAs(kAdministratorsGroupId);

    auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::NoPermissions;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked common layout when the user is logged in as admin. */
TEST_F(ResourceAccessManagerTest, checkLockedLayoutAsAdmin)
{
    loginAs(kAdministratorsGroupId);

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
    loginAs(kLiveViewersGroupId);

    auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::NoPermissions;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked common layout when the user is logged in as viewer. */
TEST_F(ResourceAccessManagerTest, checkLockedLayoutAsViewer)
{
    loginAs(kLiveViewersGroupId);

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
    loginAsCustom();
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
    loginAs(kLiveViewersGroupId);

    auto anotherUser = addUser(kLiveViewersGroupId, QnResourcePoolTestHelper::kTestUserName2);
    auto layout = createLayout(Qn::remote, false, anotherUser->getId());
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::NoPermissions;
    Qn::Permissions forbidden = Qn::FullLayoutPermissions;
    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for another viewer's layout when the user is logged in as admin. */
TEST_F(ResourceAccessManagerTest, checkNonOwnViewersLayoutAsAdmin)
{
    loginAs(kAdministratorsGroupId);

    auto anotherUser = addUser(kLiveViewersGroupId, QnResourcePoolTestHelper::kTestUserName2);
    auto layout = createLayout(Qn::remote, false, anotherUser->getId());
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::NoPermissions;
    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for another admin's layout when the user is logged in as admin. */
TEST_F(ResourceAccessManagerTest, checkNonOwnAdminsLayoutAsAdmin)
{
    loginAs(kAdministratorsGroupId);

    auto anotherUser = addUser(kAdministratorsGroupId, QnResourcePoolTestHelper::kTestUserName2);
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

    auto anotherUser = addUser(kAdministratorsGroupId, QnResourcePoolTestHelper::kTestUserName2);
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
    loginAs(kLiveViewersGroupId);

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
    loginAs(kAdministratorsGroupId);

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
    loginAs(kAdministratorsGroupId);

    auto owner = addUser(kOwnersGroupId);

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
    loginAsCustom();
    auto user = m_currentUser;
    auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);
    layout->setParentId(QnUuid());
    ASSERT_FALSE(hasPermission(user, layout, Qn::ReadPermission));
}

//-------------------------------------------------------------------------------------------------
// Checking intercom layouts

TEST_F(ResourceAccessManagerTest, checkIntercomLayoutPermissions)
{
    loginAsCustom();

    auto intercomLayout = addIntercom();
    const auto intercomId = intercomLayout->getParentId();
    setOwnAccessRights(m_currentUser->getId(), {{intercomId, AccessRight::view}});

    // Access to intercom grants access to its layout.
    checkPermissions(intercomLayout, Qn::ReadPermission, {});

    auto camera = addCamera();
    addToLayout(intercomLayout, camera);

    // Access to intercom layout doesn't grant access to its items.
    checkPermissions(camera, Qn::NoPermissions, {});
}

//-------------------------------------------------------------------------------------------------
// Checking videowall-based layouts

/** Admin can do anything with layout on videowall. */
TEST_F(ResourceAccessManagerTest, checkVideowallLayoutAsAdmin)
{
    loginAs(kAdministratorsGroupId);

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
    loginAsCustom();
    setOwnAccessRights(m_currentUser->getId(), {{kAllVideoWallsGroupId, AccessRight::view}});

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
    loginAs(kAdvancedViewersGroupId);

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
    const auto group = createUserGroup(NoGroup);
    loginAs(group.id);

    setOwnAccessRights(group.id, {{kAllVideoWallsGroupId, AccessRight::view}});

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
    loginAs(kLiveViewersGroupId);
    setOwnAccessRights(m_currentUser->getId(), {{kAllVideoWallsGroupId, AccessRight::view}});

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
    loginAs(kViewersGroupId);
    setOwnAccessRights(m_currentUser->getId(), {{kAllVideoWallsGroupId, AccessRight::view}});

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
    loginAs(kLiveViewersGroupId);
    setOwnAccessRights(m_currentUser->getId(), {{kAllVideoWallsGroupId, AccessRight::view}});

    auto videoWall = addVideoWall();
    auto ownScreen = addDesktopCamera(m_currentUser);
    auto layout = addLayout();
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
    loginAs(kViewersGroupId);

    auto videoWall = addVideoWall();
    auto ownScreen = addDesktopCamera(m_currentUser);
    auto layout = addLayout();
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
    loginAsOwner();

    auto anotherUser = addUser(kViewersGroupId);
    auto videoWall = addVideoWall();
    auto otherScreen = addDesktopCamera(anotherUser);

    setOwnAccessRights(anotherUser->getId(), {{kAllVideoWallsGroupId, AccessRight::view}});

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

    auto anotherUser = addUser(kViewersGroupId);
    auto videoWall = addVideoWall();
    auto otherScreen = addDesktopCamera(anotherUser);
    auto layout = addLayout();
    layout->setParentId(videoWall->getId());

    setOwnAccessRights(anotherUser->getId(), {{kAllVideoWallsGroupId, AccessRight::view}});

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
    loginAsOwner();

    Qn::Permissions desired = Qn::FullUserPermissions;
    Qn::Permissions forbidden = Qn::WriteNamePermission | Qn::RemovePermission | Qn::WriteAccessRightsPermission;
    desired &= ~forbidden;

    checkPermissions(m_currentUser, desired, forbidden);
}

TEST_F(ResourceAccessManagerTest, checkEditDisabledAdmin)
{
    loginAs(kAdministratorsGroupId);
    auto otherAdmin = createUser(kAdministratorsGroupId);
    otherAdmin->setEnabled(false);
    resourcePool()->addResource(otherAdmin);
    ASSERT_FALSE(hasPermission(m_currentUser, otherAdmin, Qn::WriteAccessRightsPermission));
}

TEST_F(ResourceAccessManagerTest, checkEditDisabledOwner)
{
    const auto localOwner = createUser(kOwnersGroupId, "admin", UserType::local);
    localOwner->setEnabled(false);
    resourcePool()->addResource(localOwner);

    const auto cloudOwner = createUser(kOwnersGroupId, "user@mail.com", UserType::cloud);
    resourcePool()->addResource(cloudOwner);

    ASSERT_FALSE(hasPermission(cloudOwner, localOwner, Qn::WriteAccessRightsPermission));

    UserData data;
    ec2::fromResourceToApi(localOwner, data);
    data.isEnabled = true;
    ASSERT_FALSE(resourceAccessManager()->canModifyUser(cloudOwner, localOwner, data));
}

TEST_F(ResourceAccessManagerTest, checkOwnerCanNotEditOtherOwner)
{
    const auto localOwner = createUser(kOwnersGroupId, "admin", UserType::local);
    resourcePool()->addResource(localOwner);

    const auto cloudOwner = createUser(kOwnersGroupId, "user@mail.com", UserType::cloud);
    resourcePool()->addResource(cloudOwner);

    EXPECT_FALSE(hasPermission(localOwner, cloudOwner, Qn::WriteDigestPermission));
    EXPECT_TRUE(hasPermission(cloudOwner, localOwner, Qn::WriteDigestPermission));

    for (const auto& permission: {
        Qn::WritePasswordPermission, Qn::WriteAccessRightsPermission,
        Qn::WriteEmailPermission, Qn::WriteFullNamePermission})
    {
        const auto label = QJson::serialized(permission).toStdString();
        EXPECT_FALSE(hasPermission(localOwner, cloudOwner, permission)) << label;
        EXPECT_FALSE(hasPermission(cloudOwner, localOwner, permission)) << label;
    }
}

TEST_F(ResourceAccessManagerTest, checkCanModifyUserDigestAuthorizationEnabledHimself)
{
    loginAs(kLiveViewersGroupId);
    checkCanModifyUserDigestAuthorizationEnabled(m_currentUser, /*allowed*/ false);

    loginAs(kAdministratorsGroupId);
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
    auto admin = createUser(kAdministratorsGroupId);
    admin->removeFlags(Qn::remote); //< Prevent removing of this user by loginAs();
    resourcePool()->addResource(admin);

    loginAs(kLiveViewersGroupId);
    checkCanModifyUserDigestAuthorizationEnabled(admin, /*allowed*/ true);

    loginAs(kAdministratorsGroupId);
    checkCanModifyUserDigestAuthorizationEnabled(admin, /*allowed*/ false);

    loginAsOwner();
    checkCanModifyUserDigestAuthorizationEnabled(admin, /*allowed*/ false);
}

TEST_F(ResourceAccessManagerTest, checkCanModifyUserDigestAuthorizationEnabled)
{
    auto owner = addUser(kOwnersGroupId, "owner");
    auto cloud = addUser(NoGroup, "cloud", UserType::cloud);
    auto cloudOwner =
        addUser(kOwnersGroupId, "cloud owner", UserType::cloud);
    auto admin = addUser(kAdministratorsGroupId, "admin");
    auto ldap = addUser(NoGroup, "ldap", UserType::ldap, GlobalPermission::none, "cn=ldap");
    auto other = addUser(NoGroup, "other");

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
    const auto admins = kAdministratorsGroupId;
    const auto adminsToo = createUserGroup(kAdministratorsGroupId);
    const auto owner = addUser(kOwnersGroupId, "owner");
    const auto cloud = addUser(NoGroup, "cloud", UserType::cloud);
    const auto cloudOwner = addUser(kOwnersGroupId, "cloud owner", UserType::cloud);
    const auto admin = addUser(kAdministratorsGroupId, "admin");
    const auto transitivelyInheritedAdmin = addUser(adminsToo.id, "transitively inherited admin");
    const auto ldap = addUser(NoGroup, "ldap", UserType::ldap, GlobalPermission::none, "cn=ldap");
    const auto other = addUser(NoGroup, "other");

    // Compatibility direct admin.
    const auto directAdmin = addUser(
        NoGroup, "direct admin", UserType::local, GlobalPermission::admin);

    const QnUserResourceList users = {
        owner, cloudOwner, admin, transitivelyInheritedAdmin, directAdmin, cloud, ldap, other};

    const QVector<std::pair<QnUserResourcePtr, QnUserResourcePtr>> allowedScenarios = {
        {directAdmin, directAdmin}, //< No change, considered allowed.
        {owner, admin},
        {owner, directAdmin},
        {owner, transitivelyInheritedAdmin},
        {owner, ldap},
        {owner, other},
        {owner, cloud},
        {cloudOwner, admin},
        {cloudOwner, directAdmin},
        {cloudOwner, transitivelyInheritedAdmin},
        {cloudOwner, cloud},
        {cloudOwner, ldap},
        {cloudOwner, other}};

    const QVector<std::pair<QnUserResourcePtr, QnUserResourcePtr>> allowedScenariosViaInheritance = {
        {admin, admin}, //< No change, considered allowed.
        {owner, admin},
        {owner, directAdmin},
        {owner, transitivelyInheritedAdmin},
        {owner, ldap},
        {owner, other},
        {owner, cloud},
        {cloudOwner, admin},
        {cloudOwner, directAdmin},
        {cloudOwner, transitivelyInheritedAdmin},
        {cloudOwner, cloud},
        {cloudOwner, ldap},
        {cloudOwner, other}};

    for (const auto editor: users)
    {
        for (const auto editee: users)
        {
            checkCanGrantUserAdminPermissionsViaInheritance(editor, editee, admins,
                allowedScenariosViaInheritance.contains({editor, editee}));

            // GlobalPermission::admin is still directly assignable for backward compatibility.
            checkCanGrantUserAdminPermissions(editor, editee,
                allowedScenarios.contains({editor, editee}));
        }
    }
}

TEST_F(ResourceAccessManagerTest, checkWhatUsersCanViewerCreate)
{
    loginAs(kViewersGroupId);

    // Cannot create viewers.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {kViewersGroupId}));

    // Cannot create admins.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::admin, {}));

    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {kAdministratorsGroupId}));

    // Cannot create owners.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::owner, {}));

    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {kOwnersGroupId}));
}

TEST_F(ResourceAccessManagerTest, checkWhatUsersCanAdminCreate)
{
    loginAs(kAdministratorsGroupId);

    // Can create viewers.
    ASSERT_TRUE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {kViewersGroupId}));

    // Cannot create admins.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::admin, {}));

    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {kAdministratorsGroupId}));

    // Cannot create owners.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::owner, {}));

    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {kOwnersGroupId}));
}

TEST_F(ResourceAccessManagerTest, checkWhatUsersCanOwnerCreate)
{
    loginAsOwner();

    // Can create viewers.
    ASSERT_TRUE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {kViewersGroupId}));

    // Can create admins. Direct permissions are allowed for backward compatibility.
    ASSERT_TRUE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::admin, {}));

    ASSERT_TRUE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {kAdministratorsGroupId}));

    // Cannot create owners.
    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::owner, {}));

    ASSERT_FALSE(resourceAccessManager()->canCreateUser(
        m_currentUser, GlobalPermission::none, {kOwnersGroupId}));
}

TEST_F(ResourceAccessManagerTest, checkParentGroupsValidity)
{
    loginAsOwner();

    const auto predefinedId = kViewersGroupId;
    const auto customId = createUserGroup(kAdvancedViewersGroupId).id;

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
    loginAsOwner();

    ASSERT_EQ(resourceAccessManager()->permissions(m_currentUser, UserGroupData{}),
        Qn::Permissions());
}

TEST_F(ResourceAccessManagerTest, checkOwnerAccessToGroups)
{
    loginAsOwner();

    const auto adminsGroup = createUserGroup(kAdministratorsGroupId);
    const auto nonAdminsGroup = createUserGroup(kViewersGroupId);

    ASSERT_EQ(resourceAccessManager()->permissions(m_currentUser, adminsGroup),
        kFullGroupPermissions);

    ASSERT_EQ(resourceAccessManager()->permissions(m_currentUser, nonAdminsGroup),
        kFullGroupPermissions);
}

TEST_F(ResourceAccessManagerTest, checkAdminAccessToGroups)
{
    loginAs(kAdministratorsGroupId);

    const auto adminsGroup = createUserGroup(kAdministratorsGroupId);
    const auto nonAdminsGroup = createUserGroup(kViewersGroupId);

    ASSERT_EQ(resourceAccessManager()->permissions(m_currentUser, adminsGroup),
        kReadGroupPermissions);

    ASSERT_EQ(resourceAccessManager()->permissions(m_currentUser, nonAdminsGroup),
        kFullGroupPermissions);
}

// Non-admins have no access to user groups, even no Qn::ReadPermission.
TEST_F(ResourceAccessManagerTest, checkNonAdminAccessToGroups)
{
    loginAs(kAdvancedViewersGroupId);

    const auto adminsGroup = createUserGroup(kAdministratorsGroupId);
    const auto nonAdminsGroup = createUserGroup(kViewersGroupId);

    ASSERT_EQ(resourceAccessManager()->permissions(m_currentUser, adminsGroup),
        Qn::Permissions());

    ASSERT_EQ(resourceAccessManager()->permissions(m_currentUser, nonAdminsGroup),
        Qn::Permissions());
}

TEST_F(ResourceAccessManagerTest, checkAccessToLdapGroups)
{
    loginAsOwner();

    nx::vms::api::UserGroupData ldapGroup(QnUuid::createUuid(),
        "group name",
        GlobalPermission::none,
        {kAdministratorsGroupId});

    ldapGroup.type = UserType::ldap;
    addOrUpdateUserGroup(ldapGroup);

    ASSERT_EQ(resourceAccessManager()->permissions(m_currentUser, ldapGroup),
        kFullLdapGroupPermissions);
}

TEST_F(ResourceAccessManagerTest, checkAccessToPredefinedGroups)
{
    loginAsOwner();

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
    loginAs(kAdministratorsGroupId);
    const auto target = addCamera();
    ASSERT_TRUE(hasPermission(m_currentUser, target, Qn::RemovePermission));
}

// EditCameras is not enough to be able to remove cameras
TEST_F(ResourceAccessManagerTest, checkRemoveCameraAsEditor)
{
    auto user = addUser(NoGroup);
    auto target = addCamera();

    setOwnAccessRights(user->getId(), {{target->getId(),
        AccessRight::view | AccessRight::edit}});

    ASSERT_TRUE(hasPermission(user, target, Qn::WritePermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::SavePermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::RemovePermission));
}

TEST_F(ResourceAccessManagerTest, checkViewCameraPermission)
{
    auto admin = addUser(kAdministratorsGroupId);
    auto advancedViewer = addUser(kAdvancedViewersGroupId);
    auto viewer = addUser(kViewersGroupId);
    auto live = addUser(kLiveViewersGroupId);
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
    auto admin = addUser(kAdministratorsGroupId);
    auto advancedViewer = addUser(kAdvancedViewersGroupId);
    auto viewer = addUser(kViewersGroupId);
    auto live = addUser(kLiveViewersGroupId);
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
    auto user = addUser(kAdministratorsGroupId);
    auto camera = addCamera();

    ASSERT_TRUE(hasPermission(user, camera, Qn::RemovePermission));
    resourcePool()->removeResource(user);
    ASSERT_FALSE(hasPermission(user, camera, Qn::RemovePermission));
}

TEST_F(ResourceAccessManagerTest, checkUserRoleChange)
{
    auto target = addCamera();

    auto user = addUser(NoGroup);
    ASSERT_FALSE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::ViewContentPermission));

    user->setGroupIds({kLiveViewersGroupId});
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));
}

TEST_F(ResourceAccessManagerTest, checkUserEnabledChange)
{
    auto target = addCamera();

    auto user = addUser(kLiveViewersGroupId);
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));

    user->setEnabled(false);
    ASSERT_FALSE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::ViewContentPermission));
}

TEST_F(ResourceAccessManagerTest, checkRoleAccessChange)
{
    auto target = addCamera();

    auto user = addUser(NoGroup);
    auto group = createUserGroup(NoGroup);

    user->setGroupIds({group.id});
    ASSERT_FALSE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::ViewContentPermission));

    group.parentGroupIds.push_back(kLiveViewersGroupId);
    addOrUpdateUserGroup(group);

    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));
}

TEST_F(ResourceAccessManagerTest, checkInheritedGroupAccessChange)
{
    auto target = addCamera();

    auto user = addUser(NoGroup);
    auto parentGroup = createUserGroup(NoGroup);
    auto inheritedGroup = createUserGroup(parentGroup.id);

    user->setGroupIds({inheritedGroup.id});
    ASSERT_FALSE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::ViewContentPermission));

    parentGroup.parentGroupIds = {kLiveViewersGroupId};
    addOrUpdateUserGroup(parentGroup);
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));

    inheritedGroup.parentGroupIds = {};
    addOrUpdateUserGroup(inheritedGroup);
    ASSERT_FALSE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::ViewContentPermission));

    user->setGroupIds({inheritedGroup.id, parentGroup.id});
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));

    removeUserGroup(parentGroup.id);
    ASSERT_FALSE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::ViewContentPermission));
}

TEST_F(ResourceAccessManagerTest, checkUserAndRolesCombinedPermissions)
{
    auto cameraOfParentGroup1 = addCamera();
    auto cameraOfParentGroup2 = addCamera();
    auto cameraOfInheritedGroup = addCamera();
    auto cameraOfUser = addCamera();
    auto cameraOfNoOne = addCamera();

    auto parentGroup1 = createUserGroup(NoGroup);
    setOwnAccessRights(parentGroup1.id, {{cameraOfParentGroup1->getId(), AccessRight::view}});
    EXPECT_FALSE(hasPermission(parentGroup1, cameraOfUser, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(parentGroup1, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(parentGroup1, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(parentGroup1, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(parentGroup1, cameraOfNoOne, Qn::ReadPermission));

    auto parentGroup2 = createUserGroup(NoGroup);
    setOwnAccessRights(parentGroup2.id, {{cameraOfParentGroup2->getId(), AccessRight::view}});
    EXPECT_FALSE(hasPermission(parentGroup2, cameraOfUser, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(parentGroup2, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(parentGroup2, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(parentGroup2, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(parentGroup2, cameraOfNoOne, Qn::ReadPermission));

    auto inheritedGroup = createUserGroup({parentGroup1.id, parentGroup2.id});
    setOwnAccessRights(inheritedGroup.id, {{cameraOfInheritedGroup->getId(), AccessRight::view}});
    EXPECT_FALSE(hasPermission(inheritedGroup, cameraOfUser, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(inheritedGroup, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(inheritedGroup, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(inheritedGroup, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(inheritedGroup, cameraOfNoOne, Qn::ReadPermission));

    auto user = addUser(NoGroup, "vasily");
    EXPECT_FALSE(hasPermission(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfNoOne, Qn::ReadPermission));

    setOwnAccessRights(user->getId(), {{cameraOfUser->getId(), AccessRight::view}});
    EXPECT_TRUE(hasPermission(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfNoOne, Qn::ReadPermission));

    user->setGroupIds({parentGroup1.id});
    EXPECT_TRUE(hasPermission(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(user, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfNoOne, Qn::ReadPermission));

    user->setGroupIds({parentGroup1.id, parentGroup2.id});
    EXPECT_TRUE(hasPermission(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(user, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(user, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfNoOne, Qn::ReadPermission));

    user->setGroupIds({inheritedGroup.id});
    EXPECT_TRUE(hasPermission(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(user, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(user, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(user, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfNoOne, Qn::ReadPermission));

    inheritedGroup.parentGroupIds = {};
    addOrUpdateUserGroup(inheritedGroup);
    EXPECT_TRUE(hasPermission(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_TRUE(hasPermission(user, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfNoOne, Qn::ReadPermission));

    user->setGroupIds({});
    EXPECT_TRUE(hasPermission(user, cameraOfUser, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfParentGroup1, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfParentGroup2, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfInheritedGroup, Qn::ReadPermission));
    EXPECT_FALSE(hasPermission(user, cameraOfNoOne, Qn::ReadPermission));
}

TEST_F(ResourceAccessManagerTest, checkEditAccessChange)
{
    auto target = addCamera();

    auto user = addUser(kLiveViewersGroupId);
    ASSERT_FALSE(hasPermission(user, target, Qn::SavePermission));

    setOwnAccessRights(user->getId(), {{kAllDevicesGroupId, AccessRight::edit}});
    ASSERT_TRUE(hasPermission(user, target, Qn::SavePermission));
}

TEST_F(ResourceAccessManagerTest, checkRoleRemoved)
{
    auto target = addCamera();

    auto user = addUser(NoGroup);

    auto group = createUserGroup(kLiveViewersGroupId);

    user->setGroupIds({group.id});
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));

    removeUserGroup(group.id);
    ASSERT_FALSE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::ViewContentPermission));
}

TEST_F(ResourceAccessManagerTest, checkCameraOnVideoWall)
{
    loginAs(kAdministratorsGroupId);
    auto target = addCamera();
    auto videoWall = addVideoWall();

    auto user = addUser(NoGroup);
    setOwnAccessRights(user->getId(), {{kAllVideoWallsGroupId, AccessRight::view}});

    auto layout = createLayout(Qn::remote, false, videoWall->getId());
    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videoWall->items()->addItem(vwitem);

    LayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);
    resourcePool()->addResource(layout);
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));
}

TEST_F(ResourceAccessManagerTest, checkShareLayoutToRole)
{
    loginAs(kAdministratorsGroupId);

    auto target = addCamera();

    // Create role without access
    auto group = createUserGroup(NoGroup);

    // Create user in role
    auto user = addUser(NoGroup, kTestUserName2);
    user->setGroupIds({group.id});

    // Create own layout
    auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);

    // Place a camera on it
    LayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    // Share layout to `role`
    layout->setParentId(QnUuid());
    setOwnAccessRights(group.id, {{layout->getId(), AccessRight::view}});

    // Make sure user got permissions
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
}

TEST_F(ResourceAccessManagerTest, checkShareLayoutToParentRole)
{
    loginAs(kAdministratorsGroupId);

    auto target = addCamera();

    // Create roles without access
    auto parentGroup = createUserGroup(NoGroup);
    auto inheritedGroup = createUserGroup(parentGroup.id);

    // Create user in role
    auto user = addUser(NoGroup, kTestUserName2);
    user->setGroupIds({inheritedGroup.id});

    // Create own layout
    auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);

    // Place a camera on it
    LayoutItemData item;
    item.resource.id = target->getId();
    layout->addItem(item);

    // Share layout to `parentGroup`
    layout->setParentId(QnUuid());
    setOwnAccessRights(parentGroup.id, {{layout->getId(), AccessRight::view}});

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
    auto user = addUser(kAdministratorsGroupId);
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
    auto user = addUser(kAdministratorsGroupId);
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
    auto user = addUser(kAdministratorsGroupId);
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
    auto user = addUser(kAdministratorsGroupId);
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
    loginAsCustom();
    auto server = addServer();

    Qn::Permissions desired = Qn::ReadPermission;
    Qn::Permissions forbidden = Qn::FullServerPermissions;
    forbidden &= ~desired;

    checkPermissions(server, desired, forbidden);
}

/* User with live viewer permissions can view health monitors by default. */
TEST_F(ResourceAccessManagerTest, checkServerAsLiveViewer)
{
    loginAs(kLiveViewersGroupId);

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
    loginAsCustom();

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
    loginAs(kLiveViewersGroupId);

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
    loginAsCustom();
    setOwnAccessRights(m_currentUser->getId(), {{kAllVideoWallsGroupId, AccessRight::view}});

    auto videowall = addVideoWall();

    Qn::Permissions desired = Qn::ReadWriteSavePermission | Qn::WriteNamePermission;
    Qn::Permissions forbidden = Qn::RemovePermission;

    checkPermissions(videowall, desired, forbidden);
}

/* Videowall is inaccessible for default user. */
TEST_F(ResourceAccessManagerTest, checkVideowallAsViewer)
{
    loginAs(kAdvancedViewersGroupId);

    auto videowall = addVideoWall();

    Qn::Permissions desired = Qn::NoPermissions;
    Qn::Permissions forbidden = Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    checkPermissions(videowall, desired, forbidden);
}

// ------------------------------------------------------------------------------------------------
// Permissions by id.

TEST_F(ResourceAccessManagerTest, checkPermissionsById)
{
    loginAsOwner();

    auto camera = addCamera();
    auto server = addServer();
    auto user = addUser(kViewersGroupId);
    auto group = createUserGroup(kViewersGroupId);

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

} // namespace nx::vms::common::test
