#include <gtest/gtest.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource_stub.h>

#include <nx/fusion/model_functions.h>

namespace {
const QString userName1 = QStringLiteral("unit_test_user_1");
const QString userName2 = QStringLiteral("unit_test_user_2");
}

void PrintTo(const Qn::Permissions& val, ::std::ostream* os)
{
    *os << QnLexical::serialized(val).toStdString();
}

class QnResourceAccessManagerTest: public testing::Test
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

    QnUserResourcePtr addUser(const QString &name, Qn::GlobalPermissions globalPermissions, QnUserType userType = QnUserType::Local)
    {
        QnUserResourcePtr user(new QnUserResource(userType));
        user->setId(QnUuid::createUuid());
        user->setName(name);
        user->setRawPermissions(globalPermissions);
        user->addFlags(Qn::remote);
        qnResPool->addResource(user);

        return user;
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

    QnVirtualCameraResourcePtr createCamera()
    {
        QnVirtualCameraResourcePtr camera(new QnCameraResourceStub());
        return camera;
    }

    void logout()
    {
        qnResPool->removeResources(qnResPool->getResourcesWithFlag(Qn::remote));
        m_currentUser.clear();
    }

    void loginAsOwner()
    {
        logout();
        auto user = addUser(userName1, Qn::NoGlobalPermissions);
        user->setOwner(true);
        m_currentUser = user;
    }

    void loginAs(Qn::GlobalPermissions globalPermissions, QnUserType userType = QnUserType::Local)
    {
        logout();
        auto user = addUser(userName1, globalPermissions, userType);
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

    auto anotherUser = addUser(userName2, Qn::GlobalLiveViewerPermissionSet);
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

    auto anotherUser = addUser(userName2, Qn::GlobalLiveViewerPermissionSet);
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

    auto anotherUser = addUser(userName2, Qn::GlobalAdminPermission);
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

    auto anotherUser = addUser(userName2, Qn::GlobalAdminPermission);
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