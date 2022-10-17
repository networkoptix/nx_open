// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <client/client_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/system_administration/dialogs/group_settings_dialog.h>
#include <nx/vms/client/desktop/test_support/test_context.h>

#include <qml/qml_test_environment.h>

namespace nx::vms::client::desktop {

namespace test {

class GroupSettingsDialogTest: public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        qnResourcesChangesManager->setMockImplementation(
            std::make_unique<nx::vms::client::core::RemoteConnectionAwareMock>());

        // Register color themes and image providers.
        qnClientModule->initSkin();
    }

    virtual void TearDown() override
    {
    }

    void openEditDialog(const QnUuid& groupId)
    {
        m_dialog.reset(
            new GroupSettingsDialog(
                GroupSettingsDialog::editGroup,
                m_env.systemContext(),
                /*parent*/ nullptr));
        m_dialog->setGroup(groupId);
    }

    void openCreateDialog()
    {
        m_dialog.reset(
            new GroupSettingsDialog(
                GroupSettingsDialog::createGroup,
                m_env.systemContext(),
                /*parent*/ nullptr));
        m_dialog->setGroup({});
    }

    QnUuid addGroup(const QString& name, const std::vector<QnUuid>& parents)
    {
        api::UserRoleData group;
        group.setId(QnUuid::createUuid());
        group.name = name;
        group.parentRoleIds = parents;
        m_env.systemContext()->userRolesManager()->addOrUpdateUserRole(group);
        return group.id;
    }

    bool groupWithNameExists(const QString& name)
    {
        const auto groups = m_env.systemContext()->userRolesManager()->userRoles();

        return groups.end() != std::find_if(
            groups.begin(),
            groups.end(),
            [name](const auto& g){ return g.name == name; });
    }

    QmlTestEnvironment m_env;
    std::unique_ptr<GroupSettingsDialog> m_dialog;
};

TEST_F(GroupSettingsDialogTest, editDescription)
{
    static const QString kGroupName = "Test Group";
    static const QString kGroupDescription = "Test Group";

    const auto groupId = addGroup(kGroupName, {});

    openEditDialog(groupId);

    ASSERT_EQ(kGroupName, m_dialog->window()->property("name").toString());

    m_dialog->window()->setProperty("description", kGroupDescription);
    QMetaObject::invokeMethod(m_dialog->window(), "accept");

    const auto group = m_env.systemContext()->userRolesManager()->userRole(groupId);
    ASSERT_EQ(kGroupDescription, group.description);
}

TEST_F(GroupSettingsDialogTest, createGroup)
{
    static const QString kGroupName = "Test Group";

    ASSERT_FALSE(groupWithNameExists(kGroupName));

    openCreateDialog();

    ASSERT_EQ("New Group", m_dialog->window()->property("name").toString());

    m_dialog->window()->setProperty("name", kGroupName);
    QMetaObject::invokeMethod(m_dialog->window(), "accept");

    ASSERT_TRUE(groupWithNameExists(kGroupName));
}

} // namespace test
} // namespace nx::vms::client::desktop
