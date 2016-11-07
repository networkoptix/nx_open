#include <gtest/gtest.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_pool_test_helper.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource_access/resource_access_manager.h>

#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/camera_resource_stub.h>

#include <nx/fusion/model_functions.h>

void PrintTo(const Qn::Permissions& val, ::std::ostream* os)
{
    *os << QnLexical::serialized(val).toStdString();
}

class QnResourceAccessManagerTest: public testing::Test, protected QnResourcePoolTestHelper
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_module.reset(new QnCommonModule());
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
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
        qnResPool->removeResources(qnResPool->getResourcesWithFlag(Qn::remote));
        m_currentUser.clear();
    }

    void loginAsOwner()
    {
        logout();
        auto user = addUser(Qn::NoGlobalPermissions);
        user->setOwner(true);
        m_currentUser = user;
    }

    void loginAs(Qn::GlobalPermissions globalPermissions, QnUserType userType = QnUserType::Local)
    {
        logout();
        auto user = addUser(globalPermissions, QnResourcePoolTestHelper::kTestUserName, userType);
        ASSERT_FALSE(user->isOwner());
        m_currentUser = user;
    }

    void checkPermissions(const QnResourcePtr &resource, Qn::Permissions desired, Qn::Permissions forbidden) const
    {
        Qn::Permissions actual = qnResourceAccessManager->permissions(m_currentUser, resource);
        ASSERT_EQ(desired, actual);
        ASSERT_EQ(forbidden & actual, 0);
    }

    void checkForbiddenPermissions(const QnResourcePtr &resource, Qn::Permissions forbidden) const
    {
        Qn::Permissions actual = qnResourceAccessManager->permissions(m_currentUser, resource);
        ASSERT_EQ(forbidden & actual, 0);
    }

    // Declares the variables your tests want to use.
    QSharedPointer<QnCommonModule> m_module;
    QnUserResourcePtr m_currentUser;
};

/************************************************************************/
/* Checking layouts as admin                                            */
/************************************************************************/

/** Check permissions for common layout when the user is logged in as admin. */
TEST_F(QnResourceAccessManagerTest, checkLayoutAsAdmin)
{
    loginAsOwner();

    auto layout = createLayout(Qn::remote);
    qnResPool->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = 0;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked common layout when the user is logged in as admin. */
TEST_F(QnResourceAccessManagerTest, checkLockedLayoutAsAdmin)
{
    loginAsOwner();

    auto layout = createLayout(Qn::remote, true);
    qnResPool->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for common layout when the user is logged in as admin in safe mode. */
TEST_F(QnResourceAccessManagerTest, checkLayoutAsAdminSafeMode)
{
    loginAsOwner();
    qnCommon->setReadOnly(true);

    auto layout = createLayout(Qn::remote);
    qnResPool->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked common layout when the user is logged in as admin in safe mode. */
TEST_F(QnResourceAccessManagerTest, checkLockedLayoutAsAdminSafeMode)
{
    loginAsOwner();
    qnCommon->setReadOnly(true);

    auto layout = createLayout(Qn::remote, true);
    qnResPool->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission | Qn::SavePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking own layouts as viewer                                       */
/************************************************************************/

/** Check permissions for common layout when the user is logged in as viewer. */
TEST_F(QnResourceAccessManagerTest, checkLayoutAsViewer)
{
    loginAs(Qn::GlobalLiveViewerPermissionSet);

    auto layout = createLayout(Qn::remote);
    qnResPool->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = 0;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked common layout when the user is logged in as viewer. */
TEST_F(QnResourceAccessManagerTest, checkLockedLayoutAsViewer)
{
    loginAs(Qn::GlobalLiveViewerPermissionSet);

    auto layout = createLayout(Qn::remote, true);
    qnResPool->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for common layout when the user is logged in as viewer in safe mode. */
TEST_F(QnResourceAccessManagerTest, checkLayoutAsViewerSafeMode)
{
    loginAs(Qn::GlobalLiveViewerPermissionSet);
    qnCommon->setReadOnly(true);

    auto layout = createLayout(Qn::remote);
    qnResPool->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked common layout when the user is logged in as viewer in safe mode. */
TEST_F(QnResourceAccessManagerTest, checkLockedLayoutAsViewerSafeMode)
{
    loginAs(Qn::GlobalLiveViewerPermissionSet);
    qnCommon->setReadOnly(true);

    auto layout = createLayout(Qn::remote, true);
    qnResPool->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission | Qn::SavePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking non-own remote layouts                                      */
/************************************************************************/
/** Check permissions for another viewer's layout when the user is logged in as viewer. */
TEST_F(QnResourceAccessManagerTest, checkNonOwnViewersLayoutAsViewer)
{
    loginAs(Qn::GlobalLiveViewerPermissionSet);

    auto anotherUser = addUser(Qn::GlobalLiveViewerPermissionSet,
        QnResourcePoolTestHelper::kTestUserName2);
    auto layout = createLayout(Qn::remote, false, anotherUser->getId());
    qnResPool->addResource(layout);

    Qn::Permissions desired = 0;
    Qn::Permissions forbidden = Qn::FullLayoutPermissions;
    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for another viewer's layout when the user is logged in as admin. */
TEST_F(QnResourceAccessManagerTest, checkNonOwnViewersLayoutAsAdmin)
{
    loginAs(Qn::GlobalAdminPermission);

    auto anotherUser = addUser(Qn::GlobalLiveViewerPermissionSet,
        QnResourcePoolTestHelper::kTestUserName2);
    auto layout = createLayout(Qn::remote, false, anotherUser->getId());
    qnResPool->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = 0;
    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for another admin's layout when the user is logged in as admin. */
TEST_F(QnResourceAccessManagerTest, checkNonOwnAdminsLayoutAsAdmin)
{
    loginAs(Qn::GlobalAdminPermission);

    auto anotherUser = addUser(Qn::GlobalAdminPermission, QnResourcePoolTestHelper::kTestUserName2);
    auto layout = createLayout(Qn::remote, false, anotherUser->getId());
    qnResPool->addResource(layout);

    Qn::Permissions desired = Qn::ModifyLayoutPermission;
    Qn::Permissions forbidden = Qn::FullLayoutPermissions;
    forbidden &= ~desired;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for another admin's layout when the user is logged in as owner. */
TEST_F(QnResourceAccessManagerTest, checkNonOwnAdminsLayoutAsOwner)
{
    loginAsOwner();

    auto anotherUser = addUser(Qn::GlobalAdminPermission, QnResourcePoolTestHelper::kTestUserName2);
    auto layout = createLayout(Qn::remote, false, anotherUser->getId());
    qnResPool->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = 0;
    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking shared layouts                                              */
/************************************************************************/
/** Check permissions for shared layout when the user is logged in as viewer. */
TEST_F(QnResourceAccessManagerTest, checkSharedLayoutAsViewer)
{
    loginAs(Qn::GlobalLiveViewerPermissionSet);

    auto layout = createLayout(Qn::remote);
    layout->setParentId(QnUuid());
    qnResPool->addResource(layout);

    ASSERT_TRUE(layout->isShared());

    /* By default user has no access to shared layouts. */
    Qn::Permissions desired = 0;
    Qn::Permissions forbidden = Qn::FullLayoutPermissions;
    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for shared layout when the user is logged in as admin. */
TEST_F(QnResourceAccessManagerTest, checkSharedLayoutAsAdmin)
{
    loginAs(Qn::GlobalAdminPermission);

    auto layout = createLayout(Qn::remote);
    layout->setParentId(QnUuid());
    qnResPool->addResource(layout);

    ASSERT_TRUE(layout->isShared());

    /* Admin has full access to shared layouts. */
    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = 0;
    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking videowall-based layouts                                     */
/************************************************************************/
/** Admin can do anything with layout on videowall. */
TEST_F(QnResourceAccessManagerTest, checkVideowallLayoutAsAdmin)
{
    loginAs(Qn::GlobalAdminPermission);

    auto videowall = addVideoWall();
    auto layout = createLayout(Qn::remote, false, videowall->getId());
    qnResPool->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = 0;
    checkPermissions(layout, desired, forbidden);
}

/** Videowall-controller can do anything with layout on videowall. */
TEST_F(QnResourceAccessManagerTest, checkVideowallLayoutAsVideowallUser)
{
    loginAs(Qn::GlobalControlVideoWallPermission);

    auto videowall = addVideoWall();
    auto layout = createLayout(Qn::remote, false, videowall->getId());
    qnResPool->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = 0;
    checkPermissions(layout, desired, forbidden);
}

/** Viewer can't do anything with layout on videowall. */
TEST_F(QnResourceAccessManagerTest, checkVideowallLayoutAsAdvancedViewer)
{
    loginAs(Qn::GlobalAdvancedViewerPermissionSet);

    auto videowall = addVideoWall();
    auto layout = createLayout(Qn::remote, false, videowall->getId());
    qnResPool->addResource(layout);

    Qn::Permissions desired = 0;
    Qn::Permissions forbidden = Qn::FullLayoutPermissions;
    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking user access rights                                          */
/************************************************************************/

/** Check user can edit himself (but cannot rename, remove and change access rights). */
TEST_F(QnResourceAccessManagerTest, checkUsedEditHimself)
{
    loginAsOwner();

    Qn::Permissions desired = Qn::FullUserPermissions;
    Qn::Permissions forbidden = Qn::WriteNamePermission | Qn::RemovePermission | Qn::WriteAccessRightsPermission;
    desired &= ~forbidden;

    checkPermissions(m_currentUser, desired, forbidden);
}


/************************************************************************/
/* Checking cameras access rights                                       */
/************************************************************************/

/** Check owner can remove non-owned desktop camera, but cannot view it. */
TEST_F(QnResourceAccessManagerTest, checkDesktopCameraRemove)
{
    loginAsOwner();

    auto camera = createCamera();
    camera->addFlags(Qn::desktop_camera);
    qnResPool->addResource(camera);

    Qn::Permissions desired = Qn::RemovePermission;
    Qn::Permissions forbidden = Qn::ReadPermission;
    desired &= ~forbidden;

    checkPermissions(camera, desired, forbidden);
}

TEST_F(QnResourceAccessManagerTest, checkUserRemoved)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto camera = addCamera();

    ASSERT_TRUE(qnResourceAccessManager->hasPermission(user, camera, Qn::RemovePermission));
    qnResPool->removeResource(user);
    ASSERT_FALSE(qnResourceAccessManager->hasPermission(user, camera, Qn::RemovePermission));
}

TEST_F(QnResourceAccessManagerTest, checkUserGroupChange)
{
    auto target = addCamera();

    auto user = addUser(Qn::NoGlobalPermissions);
    auto role = createRole(Qn::GlobalAccessAllMediaPermission);
    qnUserRolesManager->addOrUpdateUserRole(role);

    user->setUserGroup(role.id);
    ASSERT_TRUE(qnResourceAccessManager->hasPermission(user, target, Qn::ReadPermission));
}

TEST_F(QnResourceAccessManagerTest, checkUserEnabledChange)
{
    auto target = addCamera();

    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    ASSERT_TRUE(qnResourceAccessManager->hasPermission(user, target, Qn::ReadPermission));

    user->setEnabled(false);
    ASSERT_FALSE(qnResourceAccessManager->hasPermission(user, target, Qn::ReadPermission));
}

TEST_F(QnResourceAccessManagerTest, checkRoleAccessChange)
{
    auto target = addCamera();

    auto user = addUser(Qn::NoGlobalPermissions);
    auto role = createRole(Qn::NoGlobalPermissions);
    qnUserRolesManager->addOrUpdateUserRole(role);

    user->setUserGroup(role.id);
    ASSERT_FALSE(qnResourceAccessManager->hasPermission(user, target, Qn::ReadPermission));

    role.permissions = Qn::GlobalAccessAllMediaPermission;
    qnUserRolesManager->addOrUpdateUserRole(role);

    ASSERT_TRUE(qnResourceAccessManager->hasPermission(user, target, Qn::ReadPermission));
}

TEST_F(QnResourceAccessManagerTest, checkEditAccessChange)
{
    auto target = addCamera();

    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    ASSERT_FALSE(qnResourceAccessManager->hasPermission(user, target, Qn::SavePermission));

    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission | Qn::GlobalEditCamerasPermission);
    ASSERT_TRUE(qnResourceAccessManager->hasPermission(user, target, Qn::SavePermission));
}

TEST_F(QnResourceAccessManagerTest, checkRoleRemoved)
{
    auto target = addCamera();

    auto user = addUser(Qn::NoGlobalPermissions);
    auto role = createRole(Qn::GlobalAccessAllMediaPermission);
    qnUserRolesManager->addOrUpdateUserRole(role);

    user->setUserGroup(role.id);
    ASSERT_TRUE(qnResourceAccessManager->hasPermission(user, target, Qn::ReadPermission));

    qnUserRolesManager->removeUserRole(role.id);
    ASSERT_FALSE(qnResourceAccessManager->hasPermission(user, target, Qn::ReadPermission));
}

TEST_F(QnResourceAccessManagerTest, checkParentChanged)
{
    loginAs(Qn::NoGlobalPermissions);
    auto user = m_currentUser;
    auto layout = createLayout(Qn::remote);
    qnResPool->addResource(layout);
    layout->setParentId(QnUuid());
    ASSERT_FALSE(qnResourceAccessManager->hasPermission(user, layout, Qn::ReadPermission));
}

TEST_F(QnResourceAccessManagerTest, checkLockedChanged)
{
    loginAs(Qn::NoGlobalPermissions);
    auto user = m_currentUser;
    auto layout = createLayout(Qn::remote);
    qnResPool->addResource(layout);
    ASSERT_TRUE(qnResourceAccessManager->hasPermission(user, layout, Qn::AddRemoveItemsPermission));
    layout->setLocked(true);
    ASSERT_FALSE(qnResourceAccessManager->hasPermission(user, layout, Qn::AddRemoveItemsPermission));
}

TEST_F(QnResourceAccessManagerTest, checkEditDisabledAdmin)
{
    loginAs(Qn::GlobalAdminPermission);
    auto user = m_currentUser;
    auto otherAdmin = createUser(Qn::GlobalAdminPermission);
    otherAdmin->setEnabled(false);
    qnResPool->addResource(otherAdmin);
    ASSERT_FALSE(qnResourceAccessManager->hasPermission(user, otherAdmin, Qn::WriteAccessRightsPermission));
}

/************************************************************************/
/* Checking servers access rights                                       */
/************************************************************************/

/* Admin must have full permissions for servers. */
TEST_F(QnResourceAccessManagerTest, checkServerAsAdmin)
{
    loginAsOwner();

    auto server = addServer();

    Qn::Permissions desired = Qn::FullServerPermissions;
    Qn::Permissions forbidden = 0;

    checkPermissions(server, desired, forbidden);
}

/* All users has read-only access to server by default. */
TEST_F(QnResourceAccessManagerTest, checkServerAsViewer)
{
    loginAs(Qn::GlobalCustomUserPermission);

    auto server = addServer();

    Qn::Permissions desired = Qn::ReadPermission;
    Qn::Permissions forbidden = Qn::FullServerPermissions;
    forbidden &= ~desired;

    checkPermissions(server, desired, forbidden);
}

/* User can view health monitor if server is shared to him. */
TEST_F(QnResourceAccessManagerTest, checkAccessibleServerAsViewer)
{
    loginAs(Qn::GlobalLiveViewerPermissionSet);

    auto server = addServer();

    Qn::Permissions desired = Qn::ReadPermission | Qn::ViewContentPermission;
    Qn::Permissions forbidden = Qn::FullServerPermissions;
    forbidden &= ~desired;

    checkPermissions(server, desired, forbidden);
}


/************************************************************************/
/* Checking videowall access rights                                     */
/************************************************************************/

/* Admin must have full permissions for videowalls. */
TEST_F(QnResourceAccessManagerTest, checkVideowallAsAdmin)
{
    loginAsOwner();

    auto videowall = addVideoWall();

    Qn::Permissions desired = Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;
    Qn::Permissions forbidden = 0;

    checkPermissions(videowall, desired, forbidden);
}

/* Check admin permissions for videowalls in safe mode. */
TEST_F(QnResourceAccessManagerTest, checkVideowallAsAdminInSafeMode)
{
    loginAsOwner();
    qnCommon->setReadOnly(true);

    auto videowall = addVideoWall();

    Qn::Permissions desired = Qn::ReadPermission | Qn::WritePermission;
    Qn::Permissions forbidden = Qn::SavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    checkPermissions(videowall, desired, forbidden);
}


/* Videowall control user must have almost full permissions for videowalls. */
TEST_F(QnResourceAccessManagerTest, checkVideowallAsController)
{
    loginAs(Qn::GlobalCustomUserPermission | Qn::GlobalControlVideoWallPermission);

    auto videowall = addVideoWall();

    Qn::Permissions desired = Qn::ReadWriteSavePermission | Qn::WriteNamePermission;
    Qn::Permissions forbidden = Qn::RemovePermission;

    checkPermissions(videowall, desired, forbidden);
}

/* Videowall control user in safe mode. */
TEST_F(QnResourceAccessManagerTest, checkVideowallAsControllerInSafeMode)
{
    loginAs(Qn::GlobalCustomUserPermission | Qn::GlobalControlVideoWallPermission);
    qnCommon->setReadOnly(true);

    auto videowall = addVideoWall();

    Qn::Permissions desired = Qn::ReadPermission | Qn::WritePermission;
    Qn::Permissions forbidden = Qn::SavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    checkPermissions(videowall, desired, forbidden);
}

/* Videowall is inaccessible for default user. */
TEST_F(QnResourceAccessManagerTest, checkVideowallAsViewer)
{
    loginAs(Qn::GlobalAdvancedViewerPermissionSet);

    auto videowall = addVideoWall();

    Qn::Permissions desired = 0;
    Qn::Permissions forbidden = Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    checkPermissions(videowall, desired, forbidden);
}
