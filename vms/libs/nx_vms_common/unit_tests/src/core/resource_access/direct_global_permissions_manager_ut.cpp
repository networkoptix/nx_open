// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
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
    auto group = createUserGroup(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(hasGlobalPermission(group, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(group, GlobalPermission::userInput));

    auto user = addUser(
        NoGroup, kTestUserName, nx::vms::api::UserType::local, GlobalPermission::userInput);
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::userInput));

    user->setGroupIds({group.id});
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::userInput));

    removeUserGroup(group.id);
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::userInput));
}

TEST_F(QnDirectGlobalPermissionsManagerTest, checkMultipleRoles)
{
    auto mediaGroup = createUserGroup(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(hasGlobalPermission(mediaGroup, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(mediaGroup, GlobalPermission::userInput));

    auto inputGroup = createUserGroup(GlobalPermission::userInput);
    ASSERT_FALSE(hasGlobalPermission(inputGroup, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(inputGroup, GlobalPermission::userInput));

    auto user = addUser(NoGroup);
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::userInput));

    user->setGroupIds({mediaGroup.id});
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::userInput));

    user->setGroupIds({inputGroup.id});
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::userInput));

    user->setGroupIds({mediaGroup.id, inputGroup.id});
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::userInput));

    removeUserGroup(mediaGroup.id);
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::userInput));

    removeUserGroup(inputGroup.id);
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::userInput));
}

TEST_F(QnDirectGlobalPermissionsManagerTest, checkRoleInheritance)
{
    auto mediaGroup = createUserGroup(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(hasGlobalPermission(mediaGroup, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(mediaGroup, GlobalPermission::userInput));

    auto inputGroup = createUserGroup(GlobalPermission::userInput);
    ASSERT_FALSE(hasGlobalPermission(inputGroup, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(inputGroup, GlobalPermission::userInput));

    auto inheritedGroup = createUserGroup({mediaGroup.id, inputGroup.id});
    ASSERT_TRUE(hasGlobalPermission(inheritedGroup, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(inheritedGroup, GlobalPermission::userInput));

    auto user = addUser(NoGroup);
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::userInput));

    user->setGroupIds({inheritedGroup.id});
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::userInput));

    removeUserGroup(mediaGroup.id);
    ASSERT_FALSE(hasGlobalPermission(inheritedGroup, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_TRUE(hasGlobalPermission(inheritedGroup, GlobalPermission::userInput));
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::userInput));

    removeUserGroup(inputGroup.id);
    ASSERT_FALSE(hasGlobalPermission(inheritedGroup, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
    ASSERT_FALSE(hasGlobalPermission(inheritedGroup, GlobalPermission::userInput));
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::userInput));
}

} // namespace nx::vms::common::test
