// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <common/static_common_module.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/common/user_management/user_group_manager.h>
#include <ui/workbench/workbench_context.h>

namespace {

const QString userName1 = QStringLiteral("unit_test_user_1");
const QString userName2 = QStringLiteral("unit_test_user_2");

} // namespace

namespace nx::vms::client::desktop::test {

class AccessControllerTest: public ContextBasedTest
{
protected:
    virtual void TearDown()
    {
        m_currentUser.clear();
        ContextBasedTest::TearDown();
    }

    QnLayoutResourcePtr createLayout(
        Qn::ResourceFlags flags, bool locked = false, const QnUuid& parentId = QnUuid())
    {
        QnLayoutResourcePtr layout;
        if (flags.testFlag(Qn::exported_layout))
        {
            layout.reset(new QnFileLayoutResource({}));
            layout->setUrl("path/to/file");
        }
        else
        {
            layout.reset(new LayoutResource());
        }

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
        if (m_currentUser)
            resourcePool()->removeResource(m_currentUser);
        m_currentUser.clear();
        systemContext()->accessController()->setUser(m_currentUser);
    }

    void loginAs(const Ids& parentGroupIds)
    {
        logout();
        m_currentUser = addUser(parentGroupIds, userName1);
        systemContext()->accessController()->setUser(m_currentUser);
    }

    QnUserResourcePtr m_currentUser;
};

#define CHECK_EXACT_PERMISSIONS(resource, expected) { \
    const auto actual = systemContext()->accessController()->permissions(resource); \
    ASSERT_EQ(actual, expected); }

#define CHECK_HAS_PERMISSIONS(resource, expected) { \
    const auto actual = systemContext()->accessController()->permissions(resource); \
    ASSERT_EQ(actual & expected, expected); }

#define CHECK_DOESNT_HAVE_PERMISSIONS(resource, forbidden) { \
    const auto actual = systemContext()->accessController()->permissions(resource); \
    ASSERT_EQ(actual & forbidden, 0); }

//-------------------------------------------------------------------------------------------------
// Checking exported layouts

// Fix permissions for exported layouts (files).
TEST_F(AccessControllerTest, checkExportedLayouts)
{
    const auto layout = createLayout(Qn::exported_layout);
    resourcePool()->addResource(layout);

    ASSERT_TRUE(layout->isFile());

    // Exported layouts can be edited when we are not logged in.
    // Now it is decided to (theoretically) allow removing of layout files.
    const Qn::Permissions expected = Qn::FullLayoutPermissions;
    CHECK_EXACT_PERMISSIONS(layout, expected);

    // Result is the same even for live users.
    loginAs(api::kLiveViewersGroupId);
    CHECK_EXACT_PERMISSIONS(layout, expected);
}

// Fix permissions for exported layouts (files).
TEST_F(AccessControllerTest, checkEncryptedExportedLayouts)
{
    const auto layout = createLayout(Qn::exported_layout);
    layout.dynamicCast<QnFileLayoutResource>()->setIsEncrypted(true);
    resourcePool()->addResource(layout);

    ASSERT_TRUE(layout->isFile());

    // Encrypted layouts cannot be edited, only (theoretically) removed or (theoretically) renamed.
    const Qn::Permissions expected = Qn::ReadPermission
        | Qn::RemovePermission
        | Qn::WriteNamePermission;

    CHECK_EXACT_PERMISSIONS(layout, expected);

    // Result is the same even for live users.
    loginAs(api::kLiveViewersGroupId);
    CHECK_EXACT_PERMISSIONS(layout, expected);
}

// Fix permissions for locked exported layouts (files).
TEST_F(AccessControllerTest, checkExportedLayoutsLocked)
{
    const auto layout = createLayout(Qn::exported_layout, true);
    resourcePool()->addResource(layout);

    ASSERT_TRUE(layout->isFile());

    const Qn::Permissions desired = Qn::FullLayoutPermissions;
    const Qn::Permissions forbidden = Qn::AddRemoveItemsPermission;

    CHECK_EXACT_PERMISSIONS(layout, desired & ~forbidden);

    // Result is the same even for live users.
    loginAs(api::kLiveViewersGroupId);
    CHECK_EXACT_PERMISSIONS(layout, desired & ~forbidden);
}

//-------------------------------------------------------------------------------------------------
// Checking local layouts (not logged in)

// Check permissions for layouts when the user is not logged in.
TEST_F(AccessControllerTest, checkLocalLayoutsUnlogged)
{
    const auto layout = createLayout(Qn::local);
    resourcePool()->addResource(layout);

    // Local layouts can be edited when we are not logged id.
    const Qn::Permissions desired = Qn::FullLayoutPermissions;
    // But their name is fixed, and them cannot be saved to server,
    // removed or has their settings edited.
    const Qn::Permissions forbidden = Qn::WriteNamePermission
        | Qn::SavePermission
        | Qn::EditLayoutSettingsPermission
        | Qn::RemovePermission;

    CHECK_EXACT_PERMISSIONS(layout, desired & ~forbidden);
}

//-------------------------------------------------------------------------------------------------
// Checking unsaved layouts

// Check permissions for unsaved layouts when the user is logged in.
TEST_F(AccessControllerTest, checkLocalLayoutsLoggedIn)
{
    loginAs(api::kLiveViewersGroupId);

    const auto layout = createLayout(Qn::local);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    Qn::Permissions forbidden = Qn::RemovePermission;

    CHECK_EXACT_PERMISSIONS(layout, desired & ~forbidden);

    // Make sure nothing changes if logged in as Administrator.
    loginAs(api::kAdministratorsGroupId);
    CHECK_EXACT_PERMISSIONS(layout, desired & ~forbidden);
}

// Check permissions for locked unsaved layouts when the user is logged in.
TEST_F(AccessControllerTest, checkLockedLocalLayoutsLoggedIn)
{
    loginAs(api::kLiveViewersGroupId);

    const auto layout = createLayout(Qn::local, true);
    resourcePool()->addResource(layout);

    Qn::Permissions desired = Qn::FullLayoutPermissions;
    // Layout still can be saved.
    Qn::Permissions forbidden = Qn::RemovePermission
        | Qn::AddRemoveItemsPermission
        | Qn::WriteNamePermission;

    CHECK_EXACT_PERMISSIONS(layout, desired & ~forbidden);

    // Make sure nothing changes if logged in as Administrator.
    loginAs(api::kAdministratorsGroupId);
    CHECK_EXACT_PERMISSIONS(layout, desired & ~forbidden);
}

// Check Qn::WriteDigestPermission permission for a new cloud user (should be missing).
TEST_F(AccessControllerTest, checkPermissionForNewCloudUser)
{
    loginAs(api::kPowerUsersGroupId);

    const QnUserResourcePtr newCloudUser(
        new QnUserResource(nx::vms::api::UserType::cloud, /*externalId*/ {}));
    newCloudUser->setIdUnsafe(QnUuid::createUuid());
    resourcePool()->addResource(newCloudUser);

    CHECK_DOESNT_HAVE_PERMISSIONS(newCloudUser, Qn::WriteDigestPermission);
}

TEST_F(AccessControllerTest, userGroupChangesUpdatePermissionsForTheirMembers)
{
    loginAs(api::kPowerUsersGroupId);

    const QnUserResourcePtr newLocalUser(
        new QnUserResource(nx::vms::api::UserType::local, /*externalId*/ {}));
    newLocalUser->setName(userName2);
    newLocalUser->setIdUnsafe(QnUuid::createUuid());
    newLocalUser->setGroupIds({api::kPowerUsersGroupId});
    resourcePool()->addResource(newLocalUser);

    // Target is a member of Power Users.
    // Current user cannot modify target power user.
    ASSERT_FALSE(
        systemContext()->accessController()->hasPermissions(newLocalUser, Qn::SavePermission));

    newLocalUser->setGroupIds({});

    // Target is not a member of any group.
    // Current user can modify target non-power user.
    ASSERT_TRUE(
        systemContext()->accessController()->hasPermissions(newLocalUser, Qn::SavePermission));

    api::UserGroupData newGroup;
    newGroup.id = QnUuid::createUuid();
    newGroup.name = "test_group";
    newGroup.parentGroupIds = {api::kPowerUsersGroupId};
    systemContext()->userGroupManager()->addOrUpdate(newGroup);

    newLocalUser->setGroupIds({newGroup.id});

    // Target is member of `test_group` that is a member of Power Users.
    // Current user cannot modify target power user.
    ASSERT_FALSE(
        systemContext()->accessController()->hasPermissions(newLocalUser, Qn::SavePermission));

    newGroup.parentGroupIds = {};
    systemContext()->userGroupManager()->addOrUpdate(newGroup);

    // Target is member of `test_group` that is no longer a member of any parent group.
    // Current user can modify target non-power user.
    ASSERT_TRUE(
        systemContext()->accessController()->hasPermissions(newLocalUser, Qn::SavePermission));
}

TEST_F(AccessControllerTest, layoutPermisionsUpdatedWhenLockedOrUnlocked)
{
    loginAs(api::kAdministratorsGroupId);
    const auto layout = addLayout();
    CHECK_EXACT_PERMISSIONS(layout, Qn::FullLayoutPermissions);

    const Qn::Permissions forbiddenIfLocked = Qn::AddRemoveItemsPermission
        | Qn::WriteNamePermission
        | Qn::WritePermission
        | Qn::RemovePermission;

    layout->setLocked(true);
    CHECK_DOESNT_HAVE_PERMISSIONS(layout, forbiddenIfLocked);

    layout->setLocked(false);
    CHECK_EXACT_PERMISSIONS(layout, Qn::FullLayoutPermissions);
}

TEST_F(AccessControllerTest, cameraPermisionUpdatesBasedOnLicenseType)
{
    loginAs(api::kAdministratorsGroupId);
    const auto camera = addCamera();

    const Qn::Permissions viewPermissions = Qn::ViewLivePermission | Qn::ViewFootagePermission;
    CHECK_HAS_PERMISSIONS(camera, viewPermissions);

    camera->setProperty(ResourcePropertyKey::kForcedLicenseType,
        QString::fromStdString(nx::reflect::toString(Qn::LC_VMAX)));

    CHECK_DOESNT_HAVE_PERMISSIONS(camera, viewPermissions);

    camera->setScheduleEnabled(true);
    CHECK_HAS_PERMISSIONS(camera, viewPermissions);
}

TEST_F(AccessControllerTest, cameraRestrictionsWithDefaultPassword)
{
    loginAs(api::kAdministratorsGroupId);
    const auto camera = addCamera();
    camera->setCameraCapabilities(camera->getCameraCapabilities()
        | api::DeviceCapability::setUserPassword | api::DeviceCapability::isDefaultPassword);

    CHECK_DOESNT_HAVE_PERMISSIONS(camera, Qn::ViewLivePermission);

    camera->setCameraCapabilities(
        camera->getCameraCapabilities() & ~(int) api::DeviceCapability::setUserPassword);

    CHECK_HAS_PERMISSIONS(camera, Qn::ViewLivePermission);
}

TEST_F(AccessControllerTest, crossSystemCameraRestrictions)
{
    loginAs(api::kAdministratorsGroupId);
    const auto camera = addCamera();
    CHECK_HAS_PERMISSIONS(camera, Qn::GenericEditPermissions);

    camera->addFlags(Qn::cross_system);
    CHECK_DOESNT_HAVE_PERMISSIONS(camera, Qn::GenericEditPermissions);
}

TEST_F(AccessControllerTest, readOnlyUserRestrictions)
{
    loginAs(api::kAdministratorsGroupId);
    const auto user = addUser(api::kViewersGroupId);
    CHECK_EXACT_PERMISSIONS(user, Qn::FullUserPermissions);

    user->setAttributes(api::UserAttribute::readonly);
    CHECK_EXACT_PERMISSIONS(user, Qn::ReadPermission);
}

TEST_F(AccessControllerTest, fileLayoutPermissionUpdates)
{
    loginAs(api::kAdministratorsGroupId);

    const auto fileLayout = createLayout(Qn::exported_layout, /*locked*/ false,
        m_currentUser->getId()).objectCast<QnFileLayoutResource>();
    fileLayout->setIsEncrypted(true);
    resourcePool()->addResource(fileLayout);

    const Qn::Permissions extendedPermissions = Qn::WritePermission
        | Qn::SavePermission
        | Qn::EditLayoutSettingsPermission
        | Qn::AddRemoveItemsPermission;

    CHECK_DOESNT_HAVE_PERMISSIONS(fileLayout, extendedPermissions);

    fileLayout->usePasswordToRead("password");
    CHECK_HAS_PERMISSIONS(fileLayout, extendedPermissions);

    fileLayout->setReadOnly(true);
    CHECK_DOESNT_HAVE_PERMISSIONS(fileLayout, extendedPermissions);
}

} // namespace nx::vms::client::desktop::test
