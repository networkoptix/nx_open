// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <client_core/client_core_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>
#include <core/resource/layout_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/resource_access_manager.h>

#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

#include <nx/fusion/model_functions.h>

namespace {
const QString userName1 = QStringLiteral("unit_test_user_1");
const QString userName2 = QStringLiteral("unit_test_user_2");
}

class QnWorkbenchAccessControllerTest: public testing::Test
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_runtime.reset(new QnClientRuntimeSettings(QnStartupParameters()));
        m_staticCommon.reset(new QnStaticCommonModule());
        m_module.reset(new QnClientCoreModule(QnClientCoreModule::Mode::unitTests));
        m_resourceRuntime.reset(new QnResourceRuntimeDataManager(m_module->commonModule()));
        m_accessController.reset(new QnWorkbenchAccessController(m_module->commonModule()));
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        m_currentUser.clear();
        m_accessController.clear();
        m_resourceRuntime.clear();
        m_module.clear();
        m_staticCommon.reset();
        m_runtime.clear();
    }

    QnCommonModule* commonModule() const { return m_module->commonModule(); }
    QnResourcePool* resourcePool() const { return commonModule()->resourcePool(); }

    QnUserResourcePtr addUser(
        const QString &name, GlobalPermissions globalPermissions,
        nx::vms::api::UserType userType = nx::vms::api::UserType::local)
    {
        QnUserResourcePtr user(new QnUserResource(userType));
        user->setIdUnsafe(QnUuid::createUuid());
        user->setName(name);
        user->setRawPermissions(globalPermissions);
        user->addFlags(Qn::remote);
        resourcePool()->addResource(user);

        return user;
    }

    QnLayoutResourcePtr createLayout(Qn::ResourceFlags flags, bool locked = false, const QnUuid &parentId = QnUuid())
    {
        QnLayoutResourcePtr layout;
        if (flags.testFlag(Qn::exported_layout))
            layout.reset(new QnFileLayoutResource());
        else
            layout.reset(new QnLayoutResource());
        layout->setIdUnsafe(QnUuid::createUuid());
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
        auto user = addUser(userName1, {});
        user->setOwner(true);
        m_currentUser = user;
        m_accessController->setUser(m_currentUser);
    }

    void loginAs(
        GlobalPermissions globalPermissions,
        nx::vms::api::UserType userType = nx::vms::api::UserType::local)
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
    QSharedPointer<QnResourceRuntimeDataManager> m_resourceRuntime;
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

    // Now it is decided to (theoretically) allow removing of layout files.
    // Qn::Permissions forbidden = Qn::RemovePermission;
    Qn::Permissions forbidden = Qn::NoPermissions;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);

    // Result is the same even for live users.
    loginAs(GlobalPermission::liveViewerPermissions);
    checkPermissions(layout, desired, forbidden);
}

/** Fix permissions for exported layouts (files). */
TEST_F(QnWorkbenchAccessControllerTest, checkEncryptedExportedLayouts)
{
    auto layout = createLayout(Qn::exported_layout);
    layout->setUrl("path/to/file");
    layout.dynamicCast<QnFileLayoutResource>()->setIsEncrypted(true);
    resourcePool()->addResource(layout);

    ASSERT_TRUE(layout->isFile());

    // Encrypted layouts cannot be edited, only (theoretically) removed or (theoretically) renamed.
    Qn::Permissions desired = Qn::ReadPermission | Qn::RemovePermission | Qn::WriteNamePermission;
    Qn::Permissions forbidden = ~desired;

    checkPermissions(layout, desired, forbidden);

    // Result is the same even for live users.
    loginAs(GlobalPermission::liveViewerPermissions);
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
    // Now it is decided to (theoretically) allow removing of layout files.
    // Qn::Permissions forbidden = Qn::RemovePermission | Qn::AddRemoveItemsPermission;
    Qn::Permissions forbidden = Qn::AddRemoveItemsPermission;
    desired &= ~forbidden;

    checkPermissions(layout, desired, forbidden);

    // Result is the same even for live users.
    loginAs(GlobalPermission::liveViewerPermissions);
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
    loginAs(GlobalPermission::liveViewerPermissions);

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
    loginAs(GlobalPermission::liveViewerPermissions);

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

/** Check Qn::WriteDigestPermission permission for a new cloud user (should be missing). */
TEST_F(QnWorkbenchAccessControllerTest, checkPermissionForNewCloudUser)
{
    loginAs(GlobalPermission::adminPermissions);

    QnUserResourcePtr newCloudUser(new QnUserResource(nx::vms::api::UserType::cloud));
    newCloudUser->setIdUnsafe(QnUuid::createUuid());

    checkForbiddenPermissions(newCloudUser, Qn::WriteDigestPermission);
}
