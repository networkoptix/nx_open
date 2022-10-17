// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/client/desktop/system_administration/models/members_model.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <core/resource_management/user_roles_manager.h>


namespace nx::vms::client::desktop {

namespace test {

static const QStringList kInitialData = {
    "user1",
    "user2",
    "user3",
    "user4",
    "user5",
    "Owner",
    "Administrator",
    "Advanced Viewer",
    "Viewer",
    "Live Viewer",
    "group1",
    "    group3",
    "        user1",
    "        user2",
    "        user3",
    "        group4",
    "            user5",
    "            group5",
    "        group5",
    "group2",
    "    user2",
    "    user3",
    "    group3",
    "        user1",
    "        user2",
    "        user3",
    "        group4",
    "            user5",
    "            group5",
    "        group5",
    "group3",
    "    user1",
    "    user2",
    "    user3",
    "    group4",
    "        user5",
    "        group5",
    "    group5",
    "group4",
    "    user5",
    "    group5",
    "group5"
};

class MembersModelTest: public nx::vms::client::desktop::test::ContextBasedTest
{
public:
    virtual void SetUp() override
    {
        m_model.reset(new MembersModel(systemContext()));
        createData();
    }

    virtual void TearDown() override
    {
    }

    void createData()
    {
        auto group1 = addGroup("group1", {});
        m_group2 = addGroup("group2", {});
        m_group3 = addGroup("group3", {group1, m_group2});
        m_group4 = addGroup("group4", {m_group3});
        m_group5 = addGroup("group5", {m_group3, m_group4});

        addUser("user1", {m_group3});
        m_user2 = addUser("user2", {m_group2, m_group3});
        addUser("user3", {m_group2, m_group3});
        addUser("user4", {});
        addUser("user5", {m_group4});
    }

    QList<MembersModelGroup> parentGroups(const QnUuid& id)
    {
        std::vector<QnUuid> parentIds;

        if (const auto user = systemContext()->resourcePool()->getResourceById<QnUserResource>(id))
            parentIds = user->userRoleIds();
        else
            parentIds = systemContext()->userRolesManager()->userRole(id).parentRoleIds;

        QList<MembersModelGroup> result;

        for (const auto& groupId: parentIds)
            result << MembersModelGroup::fromId(systemContext(), groupId);

        return result;
    }

    void verifyInitialData()
    {
        EXPECT_EQ(kInitialData, visualData());
    }

    void verifyRow(int row, const QString& name, int offset)
    {
        ASSERT_EQ(name, m_model->data(m_model->index(row), Qt::DisplayRole).toString());
        ASSERT_EQ(offset, m_model->data(m_model->index(row), MembersModel::OffsetRole).toInt());
    }

    QnUuid addUser(const QString& name, const std::vector<QnUuid>& parents)
    {
        QnUserResourcePtr user(
            new QnUserResource(nx::vms::api::UserType::local, /*externalId*/ {}));
        user->setIdUnsafe(QnUuid::createUuid());
        user->setName(name);
        user->addFlags(Qn::remote);
        user->setUserRoleIds(parents);
        systemContext()->resourcePool()->addResource(user);
        return user->getId();
    }

    QnUuid addGroup(const QString& name, const std::vector<QnUuid>& parents)
    {
        api::UserRoleData group;
        group.setId(QnUuid::createUuid());
        group.name = name;
        group.parentRoleIds = parents;
        systemContext()->userRolesManager()->addOrUpdateUserRole(group);
        return group.id;
    }

    QStringList visualData()
    {
        QStringList result;
        for (int row = 0; row < m_model->rowCount(); row++)
        {
            auto text = m_model->data(m_model->index(row), Qt::DisplayRole).toString();
            auto offset = m_model->data(m_model->index(row), MembersModel::OffsetRole).toInt();

            QString offsetStr(4 * offset, ' ');
            QString line = offsetStr + text;
            result << line;
        }
        return result;
    }

    QStringList filterTopLevelByRole(int role)
    {
        QStringList result;
        const auto data = visualData();
        for (int row = 0; row < data.size(); ++row)
        {
            if (m_model->data(m_model->index(row), MembersModel::OffsetRole).toInt() > 0)
                continue;

            if (m_model->data(m_model->index(row), role).toBool())
                result << data.at(row);
        }
        return result;
    }

    void addMember(const QString& name)
    {
        auto topLevelIndex = m_model->index(visualData().indexOf(name));
        ASSERT_FALSE(m_model->data(topLevelIndex, MembersModel::IsMemberRole).toBool());
        ASSERT_TRUE(m_model->setData(topLevelIndex, true, MembersModel::IsMemberRole));
    }

    void removeMember(const QString& name)
    {
        auto topLevelIndex = m_model->index(visualData().indexOf(name));
        ASSERT_TRUE(m_model->data(topLevelIndex, MembersModel::IsMemberRole).toBool());
        ASSERT_TRUE(m_model->setData(topLevelIndex, false, MembersModel::IsMemberRole));
    }

    void addParent(const QString& name)
    {
        auto topLevelIndex = m_model->index(visualData().indexOf(name));
        ASSERT_FALSE(m_model->data(topLevelIndex, MembersModel::IsParentRole).toBool());
        ASSERT_TRUE(m_model->setData(topLevelIndex, true, MembersModel::IsParentRole));
    }

    void removeParent(const QString& name)
    {
        auto topLevelIndex = m_model->index(visualData().indexOf(name));
        ASSERT_TRUE(m_model->data(topLevelIndex, MembersModel::IsParentRole).toBool());
        ASSERT_TRUE(m_model->setData(topLevelIndex, false, MembersModel::IsParentRole));
    }

    std::unique_ptr<MembersModel> m_model;
    QnUuid m_group2;
    QnUuid m_group3;
    QnUuid m_group4;
    QnUuid m_group5;
    QnUuid m_user2;
};

TEST_F(MembersModelTest, removeAndAddParentForGroup)
{
    // Remove group4 from group3 and add it back.
    m_model->setGroupId(m_group4);

    verifyInitialData();

    removeParent("group3");

    static const QStringList kRemovedParentGroup3Data = {
        "user1",
        "user2",
        "user3",
        "user4",
        "user5",
        "Owner",
        "Administrator",
        "Advanced Viewer",
        "Viewer",
        "Live Viewer",
        "group1",
        "    group3",
        "        user1",
        "        user2",
        "        user3",
        "        group5",
        "group2",
        "    user2",
        "    user3",
        "    group3",
        "        user1",
        "        user2",
        "        user3",
        "        group5",
        "group3",
        "    user1",
        "    user2",
        "    user3",
        "    group5",
        "group4",
        "    user5",
        "    group5",
        "group5"};

    EXPECT_EQ(kRemovedParentGroup3Data, visualData());

    addParent("group3");

    EXPECT_EQ(kInitialData, visualData());
}

TEST_F(MembersModelTest, removeAndAddMemberGroup)
{
    // Remove group5 from group3 and add it back.
    m_model->setGroupId(m_group3);

    verifyInitialData();

    removeMember("group5");

    static const QStringList kRemovedMemberGroup5Data = {
        "user1",
        "user2",
        "user3",
        "user4",
        "user5",
        "Owner",
        "Administrator",
        "Advanced Viewer",
        "Viewer",
        "Live Viewer",
        "group1",
        "    group3",
        "        user1",
        "        user2",
        "        user3",
        "        group4",
        "            user5",
        "            group5",
        "group2",
        "    user2",
        "    user3",
        "    group3",
        "        user1",
        "        user2",
        "        user3",
        "        group4",
        "            user5",
        "            group5",
        "group3",
        "    user1",
        "    user2",
        "    user3",
        "    group4",
        "        user5",
        "        group5",
        "group4",
        "    user5",
        "    group5",
        "group5"};

    EXPECT_EQ(kRemovedMemberGroup5Data, visualData());

    addMember("group5");

    EXPECT_EQ(kInitialData, visualData());
}

TEST_F(MembersModelTest, removeAndAddMemberUser)
{
    // Remove user2 from group3 and add it back.
    m_model->setGroupId(m_group3);

    verifyInitialData();

    removeMember("user2");

    static const QStringList kRemovedMemberUser2Data = {
        "user1",
        "user2",
        "user3",
        "user4",
        "user5",
        "Owner",
        "Administrator",
        "Advanced Viewer",
        "Viewer",
        "Live Viewer",
        "group1",
        "    group3",
        "        user1",
        "        user3",
        "        group4",
        "            user5",
        "            group5",
        "        group5",
        "group2",
        "    user2",
        "    user3",
        "    group3",
        "        user1",
        "        user3",
        "        group4",
        "            user5",
        "            group5",
        "        group5",
        "group3",
        "    user1",
        "    user3",
        "    group4",
        "        user5",
        "        group5",
        "    group5",
        "group4",
        "    user5",
        "    group5",
        "group5"};

    EXPECT_EQ(kRemovedMemberUser2Data, visualData());

    addMember("user2");

    EXPECT_EQ(kInitialData, visualData());
}

TEST_F(MembersModelTest, removeAndAddParentForUser)
{
    // Remove user2 from group3 and add it back.
    m_model->setUserId(m_user2);

    verifyInitialData();

    removeParent("group3");

    static const QStringList kRemovedParentGroup3Data = {
        "user1",
        "user2",
        "user3",
        "user4",
        "user5",
        "Owner",
        "Administrator",
        "Advanced Viewer",
        "Viewer",
        "Live Viewer",
        "group1",
        "    group3",
        "        user1",
        "        user3",
        "        group4",
        "            user5",
        "            group5",
        "        group5",
        "group2",
        "    user2",
        "    user3",
        "    group3",
        "        user1",
        "        user3",
        "        group4",
        "            user5",
        "            group5",
        "        group5",
        "group3",
        "    user1",
        "    user3",
        "    group4",
        "        user5",
        "        group5",
        "    group5",
        "group4",
        "    user5",
        "    group5",
        "group5"};

    EXPECT_EQ(kRemovedParentGroup3Data, visualData());

    addParent("group3");

    EXPECT_EQ(kInitialData, visualData());
}

TEST_F(MembersModelTest, allowedMembers)
{
    // Check that group4 cannot have members that are already its parents (direct or inderect).
    m_model->setGroupId(m_group4);

    static const QStringList kAllowedMembers = {
        "user1",
        "user2",
        "user3",
        "user4",
        "user5",
        "group5"
    };

    const QStringList allowedMembers = filterTopLevelByRole(MembersModel::IsAllowedMember);

    ASSERT_EQ(kAllowedMembers, allowedMembers);
}

TEST_F(MembersModelTest, allowedParents)
{
    // Check that group2 cannot have parents that are already its members (direct or inderect).
    m_model->setGroupId(m_group2);

    static const QStringList kAllowedParents = {
        "Administrator",
        "Advanced Viewer",
        "Viewer",
        "Live Viewer",
        "group1"
    };

    const QStringList allowedParents = filterTopLevelByRole(MembersModel::IsAllowedParent);

    ASSERT_EQ(kAllowedParents, allowedParents);
}

TEST_F(MembersModelTest, members)
{
    // Verify direct members of group3.
    m_model->setGroupId(m_group3);

    static const QStringList kDirectMembers = {
        "user1",
        "user2",
        "user3",
        "group4",
        "group5",
    };

    const QStringList members = filterTopLevelByRole(MembersModel::IsMemberRole);

    ASSERT_EQ(kDirectMembers, members);
}

TEST_F(MembersModelTest, parents)
{
    // Verify direct parents of group5.
    m_model->setGroupId(m_group5);

    static const QStringList kDirectParents = {
        "group3",
        "group4",
    };

    const QStringList parents = filterTopLevelByRole(MembersModel::IsParentRole);

    ASSERT_EQ(kDirectParents, parents);
}

} // namespace test
} // namespace nx::vms::client::desktop
