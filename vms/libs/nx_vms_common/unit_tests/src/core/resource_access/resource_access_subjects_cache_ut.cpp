// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/test_context.h>

namespace nx::vms::common::test {

class ResourceAccessSubjectsCacheTest:
    public ContextBasedTest
{
public:
    QnResourceAccessSubjectsCache* cache() const
    {
        return systemContext()->resourceAccessSubjectsCache();
    }
};

static const auto kOwnerRoleId = QnPredefinedUserRoles::id(Qn::UserRole::owner);
static const auto kAdminRoleId = QnPredefinedUserRoles::id(Qn::UserRole::administrator);
static const auto kViewerRoleId = QnPredefinedUserRoles::id(Qn::UserRole::viewer);

TEST_F(ResourceAccessSubjectsCacheTest, PredefinedRoles)
{
    const auto u1 = addUser(GlobalPermission::none, "u1");
    EXPECT_EQ(cache()->allSubjects().size(), 1);
    EXPECT_EQ(cache()->allUsersInRole(kOwnerRoleId).size(), 0);
    EXPECT_EQ(cache()->allSubjectsInRole(kOwnerRoleId).size(), 0);
    EXPECT_EQ(cache()->allUsersInRole(kAdminRoleId).size(), 0);
    EXPECT_EQ(cache()->allSubjectsInRole(kAdminRoleId).size(), 0);
    EXPECT_EQ(cache()->allUsersInRole(kViewerRoleId).size(), 0);
    EXPECT_EQ(cache()->allSubjectsInRole(kViewerRoleId).size(), 0);

    const auto u2 = addUser(GlobalPermission::admin, "u2");
    EXPECT_EQ(cache()->allSubjects().size(), 2);
    EXPECT_EQ(cache()->allUsersInRole(kOwnerRoleId).size(), 0);
    EXPECT_EQ(cache()->allSubjectsInRole(kOwnerRoleId).size(), 0);
    EXPECT_EQ(cache()->allUsersInRole(kAdminRoleId).size(), 0);
    EXPECT_EQ(cache()->allSubjectsInRole(kAdminRoleId).size(), 0);
    EXPECT_EQ(cache()->allUsersInRole(kViewerRoleId).size(), 0);
    EXPECT_EQ(cache()->allSubjectsInRole(kViewerRoleId).size(), 0);

    u2->setUserRoleIds({kAdminRoleId});
    EXPECT_EQ(cache()->allSubjects().size(), 2);
    EXPECT_EQ(cache()->allUsersInRole(kOwnerRoleId).size(), 0);
    EXPECT_EQ(cache()->allSubjectsInRole(kOwnerRoleId).size(), 0);
    EXPECT_EQ(cache()->allUsersInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allUsersInRole(kViewerRoleId).size(), 0);
    EXPECT_EQ(cache()->allSubjectsInRole(kViewerRoleId).size(), 0);

    const auto u3 = addUser(GlobalPermission::viewerPermissions, "u3");
    EXPECT_EQ(cache()->allSubjects().size(), 3);
    EXPECT_EQ(cache()->allUsersInRole(kOwnerRoleId).size(), 0);
    EXPECT_EQ(cache()->allSubjectsInRole(kOwnerRoleId).size(), 0);
    EXPECT_EQ(cache()->allUsersInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allUsersInRole(kViewerRoleId).size(), 0);
    EXPECT_EQ(cache()->allSubjectsInRole(kViewerRoleId).size(), 0);

    u3->setUserRoleIds({kViewerRoleId});
    EXPECT_EQ(cache()->allSubjects().size(), 3);
    EXPECT_EQ(cache()->allUsersInRole(kOwnerRoleId).size(), 0);
    EXPECT_EQ(cache()->allSubjectsInRole(kOwnerRoleId).size(), 0);
    EXPECT_EQ(cache()->allUsersInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allUsersInRole(kViewerRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kViewerRoleId).size(), 1);

    const auto u4 = addUser(GlobalPermission::owner, "u4");
    EXPECT_EQ(cache()->allSubjects().size(), 4);
    EXPECT_EQ(cache()->allUsersInRole(kOwnerRoleId).size(), 0);
    EXPECT_EQ(cache()->allSubjectsInRole(kOwnerRoleId).size(), 0);
    EXPECT_EQ(cache()->allUsersInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allUsersInRole(kViewerRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kViewerRoleId).size(), 1);

    u4->setUserRoleIds({kOwnerRoleId});
    EXPECT_EQ(cache()->allSubjects().size(), 4);
    EXPECT_EQ(cache()->allUsersInRole(kOwnerRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kOwnerRoleId).size(), 1);
    EXPECT_EQ(cache()->allUsersInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allUsersInRole(kViewerRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kViewerRoleId).size(), 1);

    const auto u5 = addUser(GlobalPermission::viewerPermissions | GlobalPermission::customUser, "u5");
    EXPECT_EQ(cache()->allSubjects().size(), 5);
    EXPECT_EQ(cache()->allUsersInRole(kOwnerRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kOwnerRoleId).size(), 1);
    EXPECT_EQ(cache()->allUsersInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allUsersInRole(kViewerRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kViewerRoleId).size(), 1);

    const auto u6 = addUser(GlobalPermission::viewerPermissions, "u6");
    EXPECT_EQ(cache()->allSubjects().size(), 6);
    EXPECT_EQ(cache()->allUsersInRole(kOwnerRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kOwnerRoleId).size(), 1);
    EXPECT_EQ(cache()->allUsersInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allUsersInRole(kViewerRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kViewerRoleId).size(), 1);

    u6->setUserRoleIds({kViewerRoleId});
    EXPECT_EQ(cache()->allSubjects().size(), 6);
    EXPECT_EQ(cache()->allUsersInRole(kOwnerRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kOwnerRoleId).size(), 1);
    EXPECT_EQ(cache()->allUsersInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allUsersInRole(kViewerRoleId).size(), 2);
    EXPECT_EQ(cache()->allSubjectsInRole(kViewerRoleId).size(), 2);
}

TEST_F(ResourceAccessSubjectsCacheTest, CustomRoles)
{
    const auto u1 = addUser(GlobalPermission::none, "u1");
    const auto u2 = addUser(GlobalPermission::none, "u2");
    const auto u3 = addUser(GlobalPermission::none, "u3");
    const auto u4 = addUser(GlobalPermission::none, "u4");
    const auto u5 = addUser(GlobalPermission::none, "u5");
    const auto u6 = addUser(GlobalPermission::none, "u6");
    u2->setUserRoleIds({kAdminRoleId});
    u3->setUserRoleIds({kViewerRoleId});
    u4->setUserRoleIds({kOwnerRoleId});
    u6->setUserRoleIds({kViewerRoleId});
    EXPECT_EQ(cache()->allSubjects().size(), 6);
    EXPECT_EQ(cache()->allUsersInRole(kOwnerRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kOwnerRoleId).size(), 1);
    EXPECT_EQ(cache()->allUsersInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allUsersInRole(kViewerRoleId).size(), 2);
    EXPECT_EQ(cache()->allSubjectsInRole(kViewerRoleId).size(), 2);

    const auto r1 = createRole(GlobalPermission::none);
    const auto r2 = createRole(GlobalPermission::none, {kAdminRoleId});
    const auto r3 = createRole(GlobalPermission::none, {kViewerRoleId});
    EXPECT_EQ(cache()->allSubjects().size(), 6 + 3);
    EXPECT_EQ(cache()->allUsersInRole(kOwnerRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kOwnerRoleId).size(), 1);
    EXPECT_EQ(cache()->allUsersInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kAdminRoleId).size(), 1 + 1);
    EXPECT_EQ(cache()->allUsersInRole(kViewerRoleId).size(), 2);
    EXPECT_EQ(cache()->allSubjectsInRole(kViewerRoleId).size(), 2 + 1);

    const auto r5 = createRole(GlobalPermission::none, {r2.id, r3.id});
    EXPECT_EQ(cache()->allSubjects().size(), 6 + 4);
    EXPECT_EQ(cache()->allUsersInRole(kOwnerRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kOwnerRoleId).size(), 1);
    EXPECT_EQ(cache()->allUsersInRole(kAdminRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kAdminRoleId).size(), 1 + 2);
    EXPECT_EQ(cache()->allUsersInRole(kViewerRoleId).size(), 2);
    EXPECT_EQ(cache()->allSubjectsInRole(kViewerRoleId).size(), 2 + 2);

    u1->setUserRoleIds({r5.id});
    u6->setUserRoleIds({r1.id});
    EXPECT_EQ(cache()->allUsersInRole(kOwnerRoleId).size(), 1);
    EXPECT_EQ(cache()->allSubjectsInRole(kOwnerRoleId).size(), 1);
    EXPECT_EQ(cache()->allUsersInRole(kAdminRoleId).size(), 2);
    EXPECT_EQ(cache()->allSubjectsInRole(kAdminRoleId).size(), 2 + 2);
    EXPECT_EQ(cache()->allUsersInRole(kViewerRoleId).size(), 2);
    EXPECT_EQ(cache()->allSubjectsInRole(kViewerRoleId).size(), 2 + 2);
}

} // namespace nx::vms::common::test
