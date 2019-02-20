#include <gtest/gtest.h>

#include <common/common_module.h>

#include <nx/core/access/access_types.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_pool_test_helper.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_management/layout_tour_manager.h>

#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/shared_resources_manager.h>

#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <test_support/resource/camera_resource_stub.h>

#include <nx/fusion/model_functions.h>

class QnCachedResourceAccessManagerTest: public testing::Test, protected QnResourcePoolTestHelper
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_module.reset(new QnCommonModule(false, nx::core::access::Mode::cached));
        initializeContext(m_module.data());
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        deinitializeContext();
        m_currentUser.clear();
        m_module.clear();
    }

    QnLayoutResourcePtr createLayout(Qn::ResourceFlags flags, bool locked = false, const QnUuid &parentId = QnUuid())
    {
        QnLayoutResourcePtr layout(new QnLayoutResource());
        layout->setId(QnUuid::createUuid());
        layout->addFlags(flags);
        layout->setLocked(locked);

        if (!parentId.isNull())
            layout->setParentId(parentId);
        else if (m_currentUser)
            layout->setParentId(m_currentUser->getId());

        return layout;
    }

    void logout()
    {
        resourcePool()->removeResources(resourcePool()->getResourcesWithFlag(Qn::remote));
        m_currentUser.clear();
    }

    void loginAsOwner()
    {
        logout();
        auto user = addUser(GlobalPermission::none);
        user->setOwner(true);
        m_currentUser = user;
    }

    void loginAs(GlobalPermissions globalPermissions, QnUserType userType = QnUserType::Local)
    {
        logout();
        auto user = addUser(globalPermissions, QnResourcePoolTestHelper::kTestUserName, userType);
        ASSERT_FALSE(user->isOwner());
        m_currentUser = user;
    }

    bool hasPermission(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        Qn::Permissions requiredPermissions) const
    {
        return resourceAccessManager()->hasPermission(subject, resource, requiredPermissions);
    }

    void checkPermissions(const QnResourcePtr &resource, Qn::Permissions desired, Qn::Permissions forbidden) const
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

    // Declares the variables your tests want to use.
    QSharedPointer<QnCommonModule> m_module;
    QnUserResourcePtr m_currentUser;
};

/************************************************************************/
/* Checking layouts as admin                                            */
/************************************************************************/

/** Check permissions for common layout when the user is logged in as admin. */
TEST_F(QnCachedResourceAccessManagerTest, checkLayoutAsAdmin)
{
    loginAsOwner();

    auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = 0;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked common layout when the user is logged in as admin. */
TEST_F(QnCachedResourceAccessManagerTest, checkLockedLayoutAsAdmin)
{
    loginAsOwner();

    auto layout = createLayout(Qn::remote, true);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::WritePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for common layout when the user is logged in as admin in safe mode. */
TEST_F(QnCachedResourceAccessManagerTest, checkLayoutAsAdminSafeMode)
{
    loginAsOwner();
    commonModule()->setReadOnly(true);

    auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked common layout when the user is logged in as admin in safe mode. */
TEST_F(QnCachedResourceAccessManagerTest, checkLockedLayoutAsAdminSafeMode)
{
    loginAsOwner();
    commonModule()->setReadOnly(true);

    auto layout = createLayout(Qn::remote, true);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::WritePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission | Qn::SavePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking own layouts as viewer                                       */
/************************************************************************/

/** Check permissions for common layout when the user is logged in as viewer. */
TEST_F(QnCachedResourceAccessManagerTest, checkLayoutAsViewer)
{
    loginAs(GlobalPermission::liveViewerPermissions);

    auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = 0;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked common layout when the user is logged in as viewer. */
TEST_F(QnCachedResourceAccessManagerTest, checkLockedLayoutAsViewer)
{
    loginAs(GlobalPermission::liveViewerPermissions);

    auto layout = createLayout(Qn::remote, true);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::WritePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for common layout when the user is logged in as viewer in safe mode. */
TEST_F(QnCachedResourceAccessManagerTest, checkLayoutAsViewerSafeMode)
{
    loginAs(GlobalPermission::liveViewerPermissions);
    commonModule()->setReadOnly(true);

    auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked common layout when the user is logged in as viewer in safe mode. */
TEST_F(QnCachedResourceAccessManagerTest, checkLockedLayoutAsViewerSafeMode)
{
    loginAs(GlobalPermission::liveViewerPermissions);
    commonModule()->setReadOnly(true);

    auto layout = createLayout(Qn::remote, true);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::WritePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission | Qn::SavePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

TEST_F(QnCachedResourceAccessManagerTest, checkLockedChanged)
{
    loginAs(GlobalPermission::none);
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
TEST_F(QnCachedResourceAccessManagerTest, checkNonOwnViewersLayoutAsViewer)
{
    loginAs(GlobalPermission::liveViewerPermissions);

    auto anotherUser = addUser(GlobalPermission::liveViewerPermissions,
        QnResourcePoolTestHelper::kTestUserName2);
    auto layout = createLayout(Qn::remote, false, anotherUser->getId());
    resourcePool()->addResource(layout);

    Qn::Permissions desired = 0;
    Qn::Permissions forbidden = Qn::FullLayoutPermissions;
    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for another viewer's layout when the user is logged in as admin. */
TEST_F(QnCachedResourceAccessManagerTest, checkNonOwnViewersLayoutAsAdmin)
{
    loginAs(GlobalPermission::admin);

    auto anotherUser = addUser(GlobalPermission::liveViewerPermissions,
        QnResourcePoolTestHelper::kTestUserName2);
    auto layout = createLayout(Qn::remote, false, anotherUser->getId());
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = 0;
    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for another admin's layout when the user is logged in as admin. */
TEST_F(QnCachedResourceAccessManagerTest, checkNonOwnAdminsLayoutAsAdmin)
{
    loginAs(GlobalPermission::admin);

    auto anotherUser = addUser(GlobalPermission::admin, QnResourcePoolTestHelper::kTestUserName2);
    auto layout = createLayout(Qn::remote, false, anotherUser->getId());
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::ModifyLayoutPermission;
    Qn::Permissions forbidden = Qn::FullLayoutPermissions;
    forbidden &= ~desired;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for another admin's layout when the user is logged in as owner. */
TEST_F(QnCachedResourceAccessManagerTest, checkNonOwnAdminsLayoutAsOwner)
{
    loginAsOwner();

    auto anotherUser = addUser(GlobalPermission::admin, QnResourcePoolTestHelper::kTestUserName2);
    auto layout = createLayout(Qn::remote, false, anotherUser->getId());
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = 0;
    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking shared layouts                                              */
/************************************************************************/
/** Check permissions for shared layout when the user is logged in as viewer. */
TEST_F(QnCachedResourceAccessManagerTest, checkSharedLayoutAsViewer)
{
    loginAs(GlobalPermission::liveViewerPermissions);

    auto layout = createLayout(Qn::remote);
    layout->setParentId(QnUuid());
    resourcePool()->addResource(layout);

    ASSERT_TRUE(layout->isShared());

    /* By default user has no access to shared layouts. */
    Qn::Permissions desired = 0;
    Qn::Permissions forbidden = Qn::FullLayoutPermissions;
    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for shared layout when the user is logged in as admin. */
TEST_F(QnCachedResourceAccessManagerTest, checkSharedLayoutAsAdmin)
{
    loginAs(GlobalPermission::admin);

    auto layout = createLayout(Qn::remote);
    layout->setParentId(QnUuid());
    resourcePool()->addResource(layout);

    ASSERT_TRUE(layout->isShared());

    /* Admin has full access to shared layouts. */
    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = 0;
    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for new shared layout when the user is logged in as admin. */
TEST_F(QnCachedResourceAccessManagerTest, checkNewSharedLayoutAsAdmin)
{
    loginAs(GlobalPermission::admin);

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
    checkPermissions(layout, Qn::FullLayoutPermissions, 0);
}

TEST_F(QnCachedResourceAccessManagerTest, checkParentChanged)
{
    loginAs(GlobalPermission::none);
    auto user = m_currentUser;
    auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);
    layout->setParentId(QnUuid());
    ASSERT_FALSE(hasPermission(user, layout, Qn::ReadPermission));
}

/************************************************************************/
/* Checking videowall-based layouts                                     */
/************************************************************************/
/** Admin can do anything with layout on videowall. */
TEST_F(QnCachedResourceAccessManagerTest, checkVideowallLayoutAsAdmin)
{
    loginAs(GlobalPermission::admin);

    auto videowall = addVideoWall();
    auto layout = createLayout(Qn::remote, false, videowall->getId());
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = 0;
    checkPermissions(layout, desired, forbidden);
}

/** Videowall-controller can do anything with layout on videowall. */
TEST_F(QnCachedResourceAccessManagerTest, checkVideowallLayoutAsVideowallUser)
{
    loginAs(GlobalPermission::controlVideowall);

    auto videowall = addVideoWall();
    auto layout = createLayout(Qn::remote, false, videowall->getId());
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = 0;
    checkPermissions(layout, desired, forbidden);
}

/** Viewer can't do anything with layout on videowall. */
TEST_F(QnCachedResourceAccessManagerTest, checkVideowallLayoutAsAdvancedViewer)
{
    loginAs(GlobalPermission::advancedViewerPermissions);

    auto videowall = addVideoWall();
    auto layout = createLayout(Qn::remote, false, videowall->getId());
    resourcePool()->addResource(layout);

    Qn::Permissions desired = 0;
    Qn::Permissions forbidden = Qn::FullLayoutPermissions;
    checkPermissions(layout, desired, forbidden);
}

/** Locked layouts on videowall still can be removed if the user has control permissions. */
TEST_F(QnCachedResourceAccessManagerTest, checkVideowallLockedLayout)
{
    loginAs(GlobalPermission::controlVideowall);

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

TEST_F(QnCachedResourceAccessManagerTest, checkPushMyScreenAsViewer)
{
    loginAs(GlobalPermission::controlVideowall | GlobalPermission::viewerPermissions);

    auto camera = createCamera();
    camera->addFlags(Qn::desktop_camera);
    resourcePool()->addResource(camera);

    // User cannot see anybody's else screen.
    ASSERT_FALSE(hasPermission(m_currentUser, camera, Qn::ViewLivePermission));

    auto videowall = addVideoWall();
    auto layout = createLayout(Qn::remote, false, videowall->getId());

    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videowall->items()->addItem(vwitem);

    QnLayoutItemData item;
    item.resource.id = camera->getId();
    item.resource.uniqueId = camera->getUniqueId();
    layout->addItem(item);

    resourcePool()->addResource(layout);

    // Screen is available once it is added to videowall.
    ASSERT_TRUE(hasPermission(m_currentUser, camera, Qn::ViewLivePermission));
}


/************************************************************************/
/* Checking user access rights                                          */
/************************************************************************/

/** Check user can edit himself (but cannot rename, remove and change access rights). */
TEST_F(QnCachedResourceAccessManagerTest, checkUsedEditHimself)
{
    loginAsOwner();

    Qn::Permissions desired = Qn::FullUserPermissions;
    Qn::Permissions forbidden = Qn::WriteNamePermission | Qn::RemovePermission | Qn::WriteAccessRightsPermission;
    desired &= ~forbidden;

    checkPermissions(m_currentUser, desired, forbidden);
}

TEST_F(QnCachedResourceAccessManagerTest, checkEditDisabledAdmin)
{
    loginAs(GlobalPermission::admin);
    auto user = m_currentUser;
    auto otherAdmin = createUser(GlobalPermission::admin);
    otherAdmin->setEnabled(false);
    resourcePool()->addResource(otherAdmin);
    ASSERT_FALSE(hasPermission(user, otherAdmin, Qn::WriteAccessRightsPermission));
}

/************************************************************************/
/* Checking cameras access rights                                       */
/************************************************************************/

/** Check owner can remove non-owned desktop camera, but cannot view it. */
TEST_F(QnCachedResourceAccessManagerTest, checkDesktopCameraRemove)
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

TEST_F(QnCachedResourceAccessManagerTest, checkRemoveCameraAsAdmin)
{
    auto user = addUser(GlobalPermission::admin);
    auto target = addCamera();

    ASSERT_TRUE(hasPermission(user, target, Qn::RemovePermission));
}

// EditCameras is not enough to be able to remove cameras
TEST_F(QnCachedResourceAccessManagerTest, checkRemoveCameraAsEditor)
{
    auto user = addUser(GlobalPermission::accessAllMedia | GlobalPermission::editCameras);
    auto target = addCamera();

    ASSERT_TRUE(hasPermission(user, target, Qn::WritePermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::SavePermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::RemovePermission));
}

TEST_F(QnCachedResourceAccessManagerTest, checkViewCameraPermission)
{
    auto admin = addUser(GlobalPermission::admin);
    auto viewer = addUser(GlobalPermission::viewerPermissions);
    auto live = addUser(GlobalPermission::liveViewerPermissions);
    auto target = addCamera();

    auto viewLivePermission = Qn::ReadPermission
        | Qn::ViewContentPermission
        | Qn::ViewLivePermission;
    auto viewPermission = viewLivePermission | Qn::ViewFootagePermission;

    ASSERT_TRUE(hasPermission(admin, target, viewPermission));
    ASSERT_TRUE(hasPermission(viewer, target, viewPermission));
    ASSERT_TRUE(hasPermission(live, target, viewLivePermission));
    ASSERT_FALSE(hasPermission(live, target, Qn::ViewFootagePermission));
}


TEST_F(QnCachedResourceAccessManagerTest, checkExportCameraPermission)
{
    auto admin = addUser(GlobalPermission::admin);
    auto viewer = addUser(GlobalPermission::viewerPermissions);
    auto live = addUser(GlobalPermission::liveViewerPermissions);
    auto target = addCamera();

    auto exportPermission = Qn::ReadPermission
        | Qn::ViewContentPermission
        | Qn::ViewFootagePermission
        | Qn::ExportPermission;

    ASSERT_TRUE(hasPermission(admin, target, exportPermission));
    ASSERT_TRUE(hasPermission(viewer, target, exportPermission));
    ASSERT_FALSE(hasPermission(live, target, exportPermission));
}

TEST_F(QnCachedResourceAccessManagerTest, checkUserRemoved)
{
    auto user = addUser(GlobalPermission::admin);
    auto camera = addCamera();

    ASSERT_TRUE(hasPermission(user, camera, Qn::RemovePermission));
    resourcePool()->removeResource(user);
    ASSERT_FALSE(hasPermission(user, camera, Qn::RemovePermission));
}

TEST_F(QnCachedResourceAccessManagerTest, checkUserRoleChange)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::none);
    auto role = createRole(GlobalPermission::accessAllMedia);
    userRolesManager()->addOrUpdateUserRole(role);

    user->setUserRoleId(role.id);
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));
}

TEST_F(QnCachedResourceAccessManagerTest, checkUserEnabledChange)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));

    user->setEnabled(false);
    ASSERT_FALSE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::ViewContentPermission));
}

TEST_F(QnCachedResourceAccessManagerTest, checkRoleAccessChange)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::none);
    auto role = createRole(GlobalPermission::none);
    userRolesManager()->addOrUpdateUserRole(role);

    user->setUserRoleId(role.id);
    ASSERT_FALSE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::ViewContentPermission));

    role.permissions = GlobalPermission::accessAllMedia;
    userRolesManager()->addOrUpdateUserRole(role);

    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));
}

TEST_F(QnCachedResourceAccessManagerTest, checkEditAccessChange)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::accessAllMedia);
    ASSERT_FALSE(hasPermission(user, target, Qn::SavePermission));

    user->setRawPermissions(GlobalPermission::accessAllMedia | GlobalPermission::editCameras);
    ASSERT_TRUE(hasPermission(user, target, Qn::SavePermission));
}

TEST_F(QnCachedResourceAccessManagerTest, checkRoleRemoved)
{
    auto target = addCamera();

    auto user = addUser(GlobalPermission::none);
    auto role = createRole(GlobalPermission::accessAllMedia);
    userRolesManager()->addOrUpdateUserRole(role);

    user->setUserRoleId(role.id);
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));

    userRolesManager()->removeUserRole(role.id);
    ASSERT_FALSE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_FALSE(hasPermission(user, target, Qn::ViewContentPermission));
}

TEST_F(QnCachedResourceAccessManagerTest, checkCameraOnVideoWall)
{
    loginAs(GlobalPermission::admin);
    auto target = addCamera();
    auto videoWall = addVideoWall();
    auto user = addUser(GlobalPermission::controlVideowall);
    auto layout = createLayout(Qn::remote, false, videoWall->getId());
    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videoWall->items()->addItem(vwitem);

    QnLayoutItemData item;
    item.resource.id = target->getId();
    item.resource.uniqueId = target->getUniqueId();
    layout->addItem(item);
    resourcePool()->addResource(layout);
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
    ASSERT_TRUE(hasPermission(user, target, Qn::ViewContentPermission));
}

TEST_F(QnCachedResourceAccessManagerTest, checkShareLayoutToRole)
{
    loginAs(GlobalPermission::admin);

    auto target = addCamera();

    // Create role without access
    auto role = createRole(GlobalPermission::none);
    userRolesManager()->addOrUpdateUserRole(role);

    // Create user in role
    auto user = addUser(GlobalPermission::none, kTestUserName2);
    user->setUserRoleId(role.id);

    // Create own layout
    auto layout = createLayout(Qn::remote);
    resourcePool()->addResource(layout);

    // Place a camera on it
    QnLayoutItemData item;
    item.resource.id = target->getId();
    item.resource.uniqueId = target->getUniqueId();
    layout->addItem(item);

    // Share layout to _role_
    layout->setParentId(QnUuid());
    sharedResourcesManager()->setSharedResources(role, {layout->getId()});

    // Make sure user got permissions
    ASSERT_TRUE(hasPermission(user, target, Qn::ReadPermission));
}

/**
* VMAXes without licences:
* Viewing live: temporary allowed
* Viewing footage: temporary allowed
* Export video: temporary allowed
*/
TEST_F(QnCachedResourceAccessManagerTest, checkVMaxWithoutLicense)
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
    camera->setLicenseUsed(true);
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
TEST_F(QnCachedResourceAccessManagerTest, checkNvrWithoutLicense)
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
    camera->setLicenseUsed(true);
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewContentPermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewLivePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewFootagePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ExportPermission));
}

/**
* Camera with default auth (if can be changed) must not be viewable.
*/
TEST_F(QnCachedResourceAccessManagerTest, checkDefaultAuthCamera)
{
    auto user = addUser(GlobalPermission::admin);
    auto camera = addCamera();

    camera->setCameraCapabilities(Qn::SetUserPasswordCapability | Qn::IsDefaultPasswordCapability);

    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewContentPermission));
    ASSERT_FALSE(hasPermission(user, camera, Qn::ViewLivePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewFootagePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ExportPermission));

    // Password changed
    camera->setCameraCapabilities(Qn::SetUserPasswordCapability);
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewContentPermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewLivePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewFootagePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ExportPermission));
}

/**
* Camera with default auth must be viewable if password cannot be changed.
*/
TEST_F(QnCachedResourceAccessManagerTest, checkDefaultAuthCameraNonChangeable)
{
    auto user = addUser(GlobalPermission::admin);
    auto camera = addCamera();

    camera->setCameraCapabilities(Qn::IsDefaultPasswordCapability);
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewContentPermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewLivePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ViewFootagePermission));
    ASSERT_TRUE(hasPermission(user, camera, Qn::ExportPermission));
}

/************************************************************************/
/* Checking servers access rights                                       */
/************************************************************************/

/* Admin must have full permissions for servers. */
TEST_F(QnCachedResourceAccessManagerTest, checkServerAsAdmin)
{
    loginAsOwner();

    auto server = addServer();

    Qn::Permissions desired = Qn::FullServerPermissions;
    Qn::Permissions forbidden = 0;

    checkPermissions(server, desired, forbidden);
}

/* All users has read-only access to server by default. */
TEST_F(QnCachedResourceAccessManagerTest, checkServerAsViewer)
{
    loginAs(GlobalPermission::customUser);

    auto server = addServer();

    Qn::Permissions desired = Qn::ReadPermission;
    Qn::Permissions forbidden = Qn::FullServerPermissions;
    forbidden &= ~desired;

    checkPermissions(server, desired, forbidden);
}

/* User can view health monitor if server is shared to him. */
TEST_F(QnCachedResourceAccessManagerTest, checkAccessibleServerAsViewer)
{
    loginAs(GlobalPermission::liveViewerPermissions);

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
TEST_F(QnCachedResourceAccessManagerTest, checkStoragesAsAdmin)
{
    loginAsOwner();

    auto server = addServer();
    auto storage = addStorage(server);

    Qn::Permissions desired = Qn::ReadWriteSavePermission | Qn::RemovePermission;
    Qn::Permissions forbidden = 0;

    checkPermissions(storage, desired, forbidden);
}

/* Non-admin users should not have access to storages. */
TEST_F(QnCachedResourceAccessManagerTest, checkStoragesAsCustom)
{
    loginAs(GlobalPermission::customUser);

    auto server = addServer();
    auto storage = addStorage(server);

    Qn::Permissions desired = 0;
    Qn::Permissions forbidden = Qn::ReadWriteSavePermission | Qn::RemovePermission;

    /* We do have access to server. */
    checkPermissions(server, Qn::ReadPermission, 0);

    /* But no access to its storages. */
    checkPermissions(storage, desired, forbidden);
}

/************************************************************************/
/* Checking videowall access rights                                     */
/************************************************************************/

/* Admin must have full permissions for videowalls. */
TEST_F(QnCachedResourceAccessManagerTest, checkVideowallAsAdmin)
{
    loginAsOwner();

    auto videowall = addVideoWall();

    Qn::Permissions desired = Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;
    Qn::Permissions forbidden = 0;

    checkPermissions(videowall, desired, forbidden);
}

/* Check admin permissions for videowalls in safe mode. */
TEST_F(QnCachedResourceAccessManagerTest, checkVideowallAsAdminInSafeMode)
{
    loginAsOwner();
    commonModule()->setReadOnly(true);

    auto videowall = addVideoWall();

    Qn::Permissions desired = Qn::ReadPermission | Qn::WritePermission;
    Qn::Permissions forbidden = Qn::SavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    checkPermissions(videowall, desired, forbidden);
}


/* Videowall control user must have almost full permissions for videowalls. */
TEST_F(QnCachedResourceAccessManagerTest, checkVideowallAsController)
{
    loginAs(GlobalPermission::customUser | GlobalPermission::controlVideowall);

    auto videowall = addVideoWall();

    Qn::Permissions desired = Qn::ReadWriteSavePermission | Qn::WriteNamePermission;
    Qn::Permissions forbidden = Qn::RemovePermission;

    checkPermissions(videowall, desired, forbidden);
}

/* Videowall control user in safe mode. */
TEST_F(QnCachedResourceAccessManagerTest, checkVideowallAsControllerInSafeMode)
{
    loginAs(GlobalPermission::customUser | GlobalPermission::controlVideowall);
    commonModule()->setReadOnly(true);

    auto videowall = addVideoWall();

    Qn::Permissions desired = Qn::ReadPermission | Qn::WritePermission;
    Qn::Permissions forbidden = Qn::SavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    checkPermissions(videowall, desired, forbidden);
}

/* Videowall is inaccessible for default user. */
TEST_F(QnCachedResourceAccessManagerTest, checkVideowallAsViewer)
{
    loginAs(GlobalPermission::advancedViewerPermissions);

    auto videowall = addVideoWall();

    Qn::Permissions desired = 0;
    Qn::Permissions forbidden = Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    checkPermissions(videowall, desired, forbidden);
}
