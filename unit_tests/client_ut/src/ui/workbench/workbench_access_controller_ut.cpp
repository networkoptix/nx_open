#define QN_NO_KEYWORD_UNUSED
#include <gtest/gtest.h>

#include <common/common_module.h>

#include <client/client_module.h>
#include <client/client_runtime_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/resource_type.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>

#include <ui/style/skin.h>

#include <utils/common/model_functions.h>

namespace {
    const QString userName1 = QStringLiteral("unit_test_user_1");
    const QString userName2 = QStringLiteral("unit_test_user_2");
}



void PrintTo(const Qn::Permissions& val, ::std::ostream* os) {
    *os << QnLexical::serialized(val).toStdString();
}

class QnWorkbenchAccessControllerTest : public testing::Test {
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp() {
        m_module.reset(new QnClientModule());
        m_skin.reset(new QnSkin());
        m_context.reset(new QnWorkbenchContext(qnResPool));
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown() {
        m_context.clear();
        m_skin.clear();
        m_module.clear();
    }

    QnUserResourcePtr addUser(const QString &name, Qn::Permissions globalPermissions) {
        QnUserResourcePtr user(new QnUserResource());
        user->setId(QnUuid::createUuid());
        user->setName(name);
        user->setPermissions(globalPermissions);
        qnResPool->addResource(user);

        return user;
    }

    QnLayoutResourcePtr createLayout(Qn::ResourceFlags flags, bool locked = false, bool userCanEdit = true, const QnUuid &parentId = QnUuid()) {
        QnLayoutResourcePtr layout(new QnLayoutResource());
        layout->setId(QnUuid::createUuid());
        layout->addFlags(flags);
        layout->setLocked(locked);
        layout->setUserCanEdit(userCanEdit);

        if (!parentId.isNull())
            layout->setParentId(parentId);
        else if (m_context->user())
            layout->setParentId(m_context->user()->getId());

        return layout;
    }

    void loginAs(Qn::Permissions globalPermissions) {
        auto user = addUser(userName1, globalPermissions);
        m_context->setUserName(userName1);
    }

    void checkPermissions(const QnResourcePtr &resource, Qn::Permissions desired, Qn::Permissions forbidden) const {
        Qn::Permissions actual = m_context->accessController()->permissions(resource);
        ASSERT_EQ(desired, actual);
        ASSERT_EQ(forbidden & actual, 0);
    }

    void checkForbiddenPermissions(const QnResourcePtr &resource, Qn::Permissions forbidden) const {
        Qn::Permissions actual = m_context->accessController()->permissions(resource);
        ASSERT_EQ(forbidden & actual, 0);
    }

    // Declares the variables your tests want to use.
    QSharedPointer<QnClientModule> m_module;
    QSharedPointer<QnSkin> m_skin;
    QSharedPointer<QnWorkbenchContext> m_context;
};


/** Initial test. Check if current user is set correctly. */
TEST_F( QnWorkbenchAccessControllerTest, init )
{
    auto user = addUser(userName1, Qn::GlobalOwnerPermissions);
    m_context->setUserName(userName1);

    ASSERT_EQ(user, m_context->user());
}

/** Test for safe mode. Check if the user cannot save layout created with external permissions. */
TEST_F( QnWorkbenchAccessControllerTest, safeCannotSaveExternalPermissionsLayout )
{
    loginAs(Qn::GlobalOwnerPermissions);
    qnCommon->setReadOnly(true);

    auto layout = createLayout(0);
    layout->setData(Qn::LayoutPermissionsRole, Qn::ReadWriteSavePermission);
    qnResPool->addResource(layout);

    checkForbiddenPermissions(layout, Qn::SavePermission);
}

/************************************************************************/
/* Checking exported layouts                                            */
/************************************************************************/

/** Fix permissions for exported layouts (files). */
TEST_F( QnWorkbenchAccessControllerTest, checkExportedLayouts )
{
    auto layout = createLayout(Qn::url | Qn::local);
    layout->setUrl("path/to/file");
    qnResPool->addResource(layout);

    ASSERT_TRUE(QnWorkbenchLayoutSnapshotManager::isFile(layout));

    /* Exported layouts can be edited when we are not logged in. */
    Qn::Permissions desired = Qn::FullLayoutPermissions;
    /* But their name is fixed. */
    Qn::Permissions forbidden = Qn::WriteNamePermission | Qn::RemovePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);

    loginAs(Qn::GlobalLiveViewerPermissions);
    qnCommon->setReadOnly(true);

    /* Result is the same even for live users in safe mode. */
    checkPermissions(layout, desired, forbidden);
}

/** Fix permissions for locked exported layouts (files). */
TEST_F( QnWorkbenchAccessControllerTest, checkExportedLayoutsLocked )
{
    auto layout = createLayout(Qn::url | Qn::local, true);
    layout->setUrl("path/to/file");
    qnResPool->addResource(layout);

    ASSERT_TRUE(QnWorkbenchLayoutSnapshotManager::isFile(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::WriteNamePermission | Qn::AddRemoveItemsPermission | Qn::RemovePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);

    loginAs(Qn::GlobalLiveViewerPermissions);
    qnCommon->setReadOnly(true);

    /* Result is the same even for live users in safe mode. */
    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking local layouts (not logged in)                               */
/************************************************************************/

/** Check permissions for layouts when the user is not logged in. */
TEST_F( QnWorkbenchAccessControllerTest, checkLocalLayoutsUnlogged )
{
    auto layout = createLayout(Qn::local);
    qnResPool->addResource(layout);

    ASSERT_TRUE(m_context->snapshotManager()->isLocal(layout));
    ASSERT_TRUE(m_context->user().isNull());

    /* Exported layouts can be edited even by live users in safe mode. */
    Qn::Permissions desired = Qn::FullLayoutPermissions;
    /* But their name is fixed. */
    Qn::Permissions forbidden = Qn::WriteNamePermission | Qn::SavePermission | Qn::EditLayoutSettingsPermission | Qn::RemovePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking unsaved layouts                                             */
/************************************************************************/

/** Check permissions for unsaved layouts when the user is logged in. */
TEST_F( QnWorkbenchAccessControllerTest, checkLocalLayoutsLoggedIn )
{
    loginAs(Qn::GlobalLiveViewerPermissions);


    auto layout = createLayout(Qn::local);
    qnResPool->addResource(layout);

    ASSERT_TRUE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for unsaved layouts when the user is logged in as admin. */
TEST_F( QnWorkbenchAccessControllerTest, checkLocalLayoutsAsAdmin )
{
    loginAs(Qn::GlobalOwnerPermissions);

    auto layout = createLayout(Qn::local);
    qnResPool->addResource(layout);

    ASSERT_TRUE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked unsaved layouts when the user is logged in. */
TEST_F( QnWorkbenchAccessControllerTest, checkLockedLocalLayoutsLoggedIn )
{
    loginAs(Qn::GlobalLiveViewerPermissions);

    auto layout = createLayout(Qn::local, true);
    qnResPool->addResource(layout);

    ASSERT_TRUE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked unsaved layouts when the user is logged in as admin. */
TEST_F( QnWorkbenchAccessControllerTest, checkLockedLocalLayoutsAsAdmin )
{
    loginAs(Qn::GlobalOwnerPermissions);

    auto layout = createLayout(Qn::local, true);
    qnResPool->addResource(layout);

    ASSERT_TRUE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking unsaved layouts in safe mode                                */
/************************************************************************/

/** Check permissions for unsaved layouts when the user is logged in (safemode). */
TEST_F( QnWorkbenchAccessControllerTest, checkLocalLayoutsLoggedInSafeMode )
{
    loginAs(Qn::GlobalLiveViewerPermissions);
    qnCommon->setReadOnly(true);

    auto layout = createLayout(Qn::local);
    qnResPool->addResource(layout);

    ASSERT_TRUE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for unsaved layouts when the user is logged in as admin (safemode). */
TEST_F( QnWorkbenchAccessControllerTest, checkLocalLayoutsAsAdminSafeMode )
{
    loginAs(Qn::GlobalOwnerPermissions);
    qnCommon->setReadOnly(true);

    auto layout = createLayout(Qn::local);
    qnResPool->addResource(layout);

    ASSERT_TRUE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked unsaved layouts when the user is logged in (safemode). */
TEST_F( QnWorkbenchAccessControllerTest, checkLockedLocalLayoutsLoggedInSafeMode )
{
    loginAs(Qn::GlobalLiveViewerPermissions);
    qnCommon->setReadOnly(true);

    auto layout = createLayout(Qn::local, true);
    qnResPool->addResource(layout);

    ASSERT_TRUE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission | Qn::SavePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked unsaved layouts when the user is logged in as admin (safemode). */
TEST_F( QnWorkbenchAccessControllerTest, checkLockedLocalLayoutsAsAdminSafeMode )
{
    loginAs(Qn::GlobalOwnerPermissions);
    qnCommon->setReadOnly(true);

    auto layout = createLayout(Qn::local, true);
    qnResPool->addResource(layout);

    ASSERT_TRUE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission | Qn::SavePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking remote (saved) layouts as admin                             */
/************************************************************************/

/** Check permissions for common layout when the user is logged in as admin. */
TEST_F( QnWorkbenchAccessControllerTest, checkRemoteLayoutAsAdmin )
{
    loginAs(Qn::GlobalOwnerPermissions);

    auto layout = createLayout(Qn::remote);
    qnResPool->addResource(layout);

    ASSERT_FALSE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = 0;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked common layout when the user is logged in as admin. */
TEST_F( QnWorkbenchAccessControllerTest, checkLockedRemoteLayoutAsAdmin )
{
    loginAs(Qn::GlobalOwnerPermissions);

    auto layout = createLayout(Qn::remote, true);
    qnResPool->addResource(layout);

    ASSERT_FALSE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for common layout when the user is logged in as admin (safemode). */
TEST_F( QnWorkbenchAccessControllerTest, checkRemoteLayoutAsAdminSafeMode )
{
    loginAs(Qn::GlobalOwnerPermissions);
    qnCommon->setReadOnly(true);

    auto layout = createLayout(Qn::remote);
    qnResPool->addResource(layout);

    ASSERT_FALSE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked common layout when the user is logged in as admin (safemode). */
TEST_F( QnWorkbenchAccessControllerTest, checkLockedRemoteLayoutAsAdminSafeMode )
{
    loginAs(Qn::GlobalOwnerPermissions);
    qnCommon->setReadOnly(true);

    auto layout = createLayout(Qn::remote, true);
    qnResPool->addResource(layout);

    ASSERT_FALSE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission | Qn::SavePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking own remote (saved) layouts as viewer                        */
/************************************************************************/

/** Check permissions for common layout when the user is logged in as viewer. */
TEST_F( QnWorkbenchAccessControllerTest, checkRemoteLayoutAsViewer )
{
    loginAs(Qn::GlobalLiveViewerPermissions);

    auto layout = createLayout(Qn::remote);
    qnResPool->addResource(layout);

    ASSERT_FALSE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = 0;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked common layout when the user is logged in as viewer. */
TEST_F( QnWorkbenchAccessControllerTest, checkLockedRemoteLayoutAsViewer )
{
    loginAs(Qn::GlobalLiveViewerPermissions);

    auto layout = createLayout(Qn::remote, true);
    qnResPool->addResource(layout);

    ASSERT_FALSE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for common layout when the user is logged in as viewer (safemode). */
TEST_F( QnWorkbenchAccessControllerTest, checkRemoteLayoutAsViewerSafeMode )
{
    loginAs(Qn::GlobalLiveViewerPermissions);
    qnCommon->setReadOnly(true);

    auto layout = createLayout(Qn::remote);
    qnResPool->addResource(layout);

    ASSERT_FALSE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked common layout when the user is logged in as viewer (safemode). */
TEST_F( QnWorkbenchAccessControllerTest, checkLockedRemoteLayoutAsViewerSafeMode )
{
    loginAs(Qn::GlobalLiveViewerPermissions);
    qnCommon->setReadOnly(true);

    auto layout = createLayout(Qn::remote, true);
    qnResPool->addResource(layout);

    ASSERT_FALSE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission | Qn::SavePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking own remote (saved) layouts, created by admin, as viewer     */
/************************************************************************/

/** Check permissions for common layout when the user is logged in as viewer. */
TEST_F( QnWorkbenchAccessControllerTest, checkReadOnlyRemoteLayoutAsViewer )
{
    loginAs(Qn::GlobalLiveViewerPermissions);

    auto layout = createLayout(Qn::remote, false, false);
    qnResPool->addResource(layout);

    ASSERT_FALSE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked common layout when the user is logged in as viewer. */
TEST_F( QnWorkbenchAccessControllerTest, checkReadOnlyLockedRemoteLayoutAsViewer )
{
    loginAs(Qn::GlobalLiveViewerPermissions);

    auto layout = createLayout(Qn::remote, true, false);
    qnResPool->addResource(layout);

    ASSERT_FALSE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission | Qn::SavePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for common layout when the user is logged in as viewer (safemode). */
TEST_F( QnWorkbenchAccessControllerTest, checkReadOnlyRemoteLayoutAsViewerSafeMode )
{
    loginAs(Qn::GlobalLiveViewerPermissions);
    qnCommon->setReadOnly(true);

    auto layout = createLayout(Qn::remote, false, false);
    qnResPool->addResource(layout);

    ASSERT_FALSE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked common layout when the user is logged in as viewer (safemode). */
TEST_F( QnWorkbenchAccessControllerTest, checkReadOnlyLockedRemoteLayoutAsViewerSafeMode )
{
    loginAs(Qn::GlobalLiveViewerPermissions);
    qnCommon->setReadOnly(true);

    auto layout = createLayout(Qn::remote, true, false);
    qnResPool->addResource(layout);

    ASSERT_FALSE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission | Qn::SavePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking non-own remote (saved) layouts as viewer                    */
/************************************************************************/
/** Check permissions for locked common layout when the user is logged in as viewer (safemode). */
TEST_F( QnWorkbenchAccessControllerTest, checkNonOwnRemoteLayoutAsViewer )
{
    loginAs(Qn::GlobalLiveViewerPermissions);

    auto anotherUser = addUser(userName2, Qn::GlobalLiveViewerPermissions);
    auto layout = createLayout(Qn::remote, false, true, anotherUser->getId());
    qnResPool->addResource(layout);

    ASSERT_FALSE(m_context->snapshotManager()->isLocal(layout));

    Qn::Permissions desired = 0;
    Qn::Permissions forbidden = Qn::FullLayoutPermissions;
    checkPermissions(layout, desired, forbidden);
}
