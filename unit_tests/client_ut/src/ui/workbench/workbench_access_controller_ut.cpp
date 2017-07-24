#include <gtest/gtest.h>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <client/client_runtime_settings.h>
#include <client_core/client_core_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

#include <nx/fusion/model_functions.h>

namespace {
const QString userName1 = QStringLiteral("unit_test_user_1");
const QString userName2 = QStringLiteral("unit_test_user_2");
}

void PrintTo(const Qn::Permissions& val, ::std::ostream* os)
{
    *os << QnLexical::serialized(val).toStdString();
}

class QnWorkbenchAccessControllerTest: public testing::Test
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_staticCommon.reset(new QnStaticCommonModule());
        m_module.reset(new QnClientCoreModule());
        m_runtime.reset(new QnClientRuntimeSettings());
        m_accessController.reset(new QnWorkbenchAccessController(m_module.data()));
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        m_currentUser.clear();
        m_accessController.clear();
        m_runtime.clear();
        m_module.clear();
        m_staticCommon.reset();
    }

    QnCommonModule* commonModule() const { return m_module->commonModule(); }
    QnResourcePool* resourcePool() const { return commonModule()->resourcePool(); }

    QnUserResourcePtr addUser(const QString &name, Qn::GlobalPermissions globalPermissions, QnUserType userType = QnUserType::Local)
    {
        QnUserResourcePtr user(new QnUserResource(userType));
        user->setId(QnUuid::createUuid());
        user->setName(name);
        user->setRawPermissions(globalPermissions);
        user->addFlags(Qn::remote);
        resourcePool()->addResource(user);

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

    void logout()
    {
        resourcePool()->removeResources(resourcePool()->getResourcesWithFlag(Qn::remote));
        m_currentUser.clear();
        m_accessController->setUser(m_currentUser);
    }

    void loginAsOwner()
    {
        logout();
        auto user = addUser(userName1, Qn::NoGlobalPermissions);
        user->setOwner(true);
        m_currentUser = user;
        m_accessController->setUser(m_currentUser);
    }

    void loginAs(Qn::GlobalPermissions globalPermissions, QnUserType userType = QnUserType::Local)
    {
        logout();
        auto user = addUser(userName1, globalPermissions, userType);
        ASSERT_FALSE(user->isOwner());
        m_currentUser = user;
        m_accessController->setUser(m_currentUser);
    }

    void checkPermissions(const QnResourcePtr &resource, Qn::Permissions desired, Qn::Permissions forbidden) const
    {
        Qn::Permissions actual = m_accessController->permissions(resource);
        ASSERT_EQ(desired, actual);
        ASSERT_EQ(forbidden & actual, 0);
    }

    void checkForbiddenPermissions(const QnResourcePtr &resource, Qn::Permissions forbidden) const
    {
        Qn::Permissions actual = m_accessController->permissions(resource);
        ASSERT_EQ(forbidden & actual, 0);
    }

    // Declares the variables your tests want to use.
    QScopedPointer<QnStaticCommonModule> m_staticCommon;
    QSharedPointer<QnClientCoreModule> m_module;
    QSharedPointer<QnWorkbenchAccessController> m_accessController;
    QSharedPointer<QnClientRuntimeSettings> m_runtime;
    QnUserResourcePtr m_currentUser;
};

/************************************************************************/
/* Checking exported layouts                                            */
/************************************************************************/

/** Fix permissions for exported layouts (files). */
TEST_F(QnWorkbenchAccessControllerTest, checkExportedLayouts)
{
    auto layout = createLayout(Qn::exported_layout);
    layout->setUrl("path/to/file");
    resourcePool()->addResource(layout);

    ASSERT_TRUE(layout->isFile());

    /* Exported layouts can be edited when we are not logged in. */
    Qn::Permissions desired = Qn::FullLayoutPermissions;
    /* But their name is fixed. */
    Qn::Permissions forbidden = Qn::RemovePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);

    /* Result is the same even for live users... */
    loginAs(Qn::GlobalLiveViewerPermissionSet);
    checkPermissions(layout, desired, forbidden);

    /* ...even in safe mode. */
    commonModule()->setReadOnly(true);
    checkPermissions(layout, desired, forbidden);
}

/** Fix permissions for locked exported layouts (files). */
TEST_F(QnWorkbenchAccessControllerTest, checkExportedLayoutsLocked)
{
    auto layout = createLayout(Qn::exported_layout, true);
    layout->setUrl("path/to/file");
    resourcePool()->addResource(layout);

    ASSERT_TRUE(layout->isFile());

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::AddRemoveItemsPermission | Qn::RemovePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);

    /* Result is the same even for live users... */
    loginAs(Qn::GlobalLiveViewerPermissionSet);
    checkPermissions(layout, desired, forbidden);

    /* ...even in safe mode. */
    commonModule()->setReadOnly(true);
    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking local layouts (not logged in)                               */
/************************************************************************/

/** Check permissions for layouts when the user is not logged in. */
TEST_F(QnWorkbenchAccessControllerTest, checkLocalLayoutsUnlogged)
{
    auto layout = createLayout(Qn::local);
    resourcePool()->addResource(layout);

    /* Local layouts can be edited when we are not logged id. */
    Qn::Permissions desired = Qn::FullLayoutPermissions;
    /* But their name is fixed, and them cannot be saved to server, removed or has their settings edited. */
    Qn::Permissions forbidden = Qn::WriteNamePermission | Qn::SavePermission | Qn::EditLayoutSettingsPermission | Qn::RemovePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking unsaved layouts                                             */
/************************************************************************/

/** Check permissions for unsaved layouts when the user is logged in. */
TEST_F(QnWorkbenchAccessControllerTest, checkLocalLayoutsLoggedIn)
{
    loginAs(Qn::GlobalLiveViewerPermissionSet);

    auto layout = createLayout(Qn::local);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);

    /* Make sure nothing changes if logged in as owner. */
    loginAsOwner();
    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked unsaved layouts when the user is logged in. */
TEST_F(QnWorkbenchAccessControllerTest, checkLockedLocalLayoutsLoggedIn)
{
    loginAs(Qn::GlobalLiveViewerPermissionSet);

    auto layout = createLayout(Qn::local, true);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    /* Layout still can be saved. */
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);

    /* Make sure nothing changes if logged in as owner. */
    loginAsOwner();
    checkPermissions(layout, desired, forbidden);
}

/************************************************************************/
/* Checking unsaved layouts in safe mode                                */
/************************************************************************/

/** Check permissions for unsaved layouts when the user is logged in safe mode. */
TEST_F(QnWorkbenchAccessControllerTest, checkLocalLayoutsLoggedInSafeMode)
{
    loginAs(Qn::GlobalLiveViewerPermissionSet);
    commonModule()->setReadOnly(true);

    auto layout = createLayout(Qn::local);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);

    /* Make sure nothing changes if logged in as owner. */
    loginAsOwner();
    checkPermissions(layout, desired, forbidden);
}

/** Check permissions for locked unsaved layouts when the user is logged in safe mode. */
TEST_F(QnWorkbenchAccessControllerTest, checkLockedLocalLayoutsLoggedInSafeMode)
{
    loginAs(Qn::GlobalLiveViewerPermissionSet);
    commonModule()->setReadOnly(true);

    auto layout = createLayout(Qn::local, true);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission | Qn::SavePermission | Qn::EditLayoutSettingsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);

    /* Make sure nothing changes if logged in as owner. */
    loginAsOwner();
    checkPermissions(layout, desired, forbidden);
}
