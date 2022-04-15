// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/core/access/access_types.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
#include <nx/vms/common/test_support/test_context.h>

namespace nx::vms::common::test {

class QnDirectGlobalPermissionsManagerTest: public ContextBasedTest
{
protected:
    virtual void TearDown()
    {
        m_currentUser.clear();
    }

    bool hasGlobalPermission(const QnResourceAccessSubject& subject,
        GlobalPermission requiredPermission)
    {
        return globalPermissionsManager()->hasGlobalPermission(subject, requiredPermission);
    }

    QnUserResourcePtr m_currentUser;
};

TEST_F(QnDirectGlobalPermissionsManagerTest, checkSingleRole)
{
    auto role = createRole(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(hasGlobalPermission(role, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(role, GlobalPermission::userInput));

    auto user = addUser(GlobalPermission::userInput);
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::userInput));

    user->setUserRoleIds({role.id});
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::userInput));

    userRolesManager()->removeUserRole(role.id);
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::userInput));
}

TEST_F(QnDirectGlobalPermissionsManagerTest, checkMultipleRoles)
{
    auto mediaRole = createRole(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(hasGlobalPermission(mediaRole, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(mediaRole, GlobalPermission::userInput));

    auto inputRole = createRole(GlobalPermission::userInput);
    ASSERT_FALSE(hasGlobalPermission(inputRole, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(inputRole, GlobalPermission::userInput));

    auto user = addUser(GlobalPermission::none);
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::userInput));

    user->setUserRoleIds({mediaRole.id});
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::userInput));

    user->setUserRoleIds({inputRole.id});
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::userInput));

    user->setUserRoleIds({mediaRole.id, inputRole.id});
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::userInput));

    userRolesManager()->removeUserRole(mediaRole.id);
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::userInput));

    userRolesManager()->removeUserRole(inputRole.id);
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::userInput));
}

TEST_F(QnDirectGlobalPermissionsManagerTest, checkRoleInheritance)
{
    auto mediaRole = createRole(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(hasGlobalPermission(mediaRole, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(mediaRole, GlobalPermission::userInput));

    auto inputRole = createRole(GlobalPermission::userInput);
    ASSERT_FALSE(hasGlobalPermission(inputRole, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(inputRole, GlobalPermission::userInput));

    auto inheritedRole = createRole(GlobalPermission::none, {mediaRole.id, inputRole.id});
    ASSERT_TRUE(hasGlobalPermission(inheritedRole, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(inheritedRole, GlobalPermission::userInput));

    auto user = addUser(GlobalPermission::none);
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::userInput));

    user->setUserRoleIds({inheritedRole.id});
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::userInput));

    userRolesManager()->removeUserRole(mediaRole.id);
    ASSERT_FALSE(hasGlobalPermission(inheritedRole, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(inheritedRole, GlobalPermission::userInput));
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::userInput));

    userRolesManager()->removeUserRole(inputRole.id);
    ASSERT_FALSE(hasGlobalPermission(inheritedRole, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(inheritedRole, GlobalPermission::userInput));
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::userInput));
}

} // namespace nx::vms::common::test
