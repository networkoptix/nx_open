// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/core/access/access_types.h>
#include <nx/reflect/json/serializer.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
#include <nx/vms/common/test_support/test_context.h>

namespace nx::vms::common::test {

class QnCachedGlobalPermissionsManagerTest: public ContextBasedTest
{
public:
    QnCachedGlobalPermissionsManagerTest():
        ContextBasedTest(nx::core::access::Mode::cached)
    {
    }

protected:
    virtual void SetUp()
    {
        QObject::connect(globalPermissionsManager(),
            &QnGlobalPermissionsManager::globalPermissionsChanged,
            [this](const QnResourceAccessSubject& subject, GlobalPermissions value)
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                m_notifiedAccess[subject.id()] = value;
                m_condition.wakeAll();
            });
    }

    bool expectPermissions(const QnResourceAccessSubject& subject, GlobalPermissions expected)
    {
        const auto actual = globalPermissionsManager()->globalPermissions(subject);
        NX_DEBUG(this, "%1 global permissions %2", subject, nx::reflect::json::serialize(actual));
        if (expected != actual)
            return false;

        NX_MUTEX_LOCKER lock(&m_mutex);
        while (m_notifiedAccess[subject.id()] != expected)
        {
            if (!m_condition.wait(&m_mutex, std::chrono::seconds(5)))
            {
                NX_DEBUG(this, "%1 notified global permissions %2", subject,
                    nx::reflect::json::serialize(m_notifiedAccess[subject.id()]));
                return false;
            }
        }

        return true;
    }

private:
    nx::Mutex m_mutex;
    nx::WaitCondition m_condition;
    std::map<QnUuid, GlobalPermissions> m_notifiedAccess;
};

TEST_F(QnCachedGlobalPermissionsManagerTest, checkSingleRole)
{
    auto role = createRole(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(expectPermissions(role, GlobalPermission::accessAllMedia));

    auto user = addUser(GlobalPermission::userInput);
    ASSERT_TRUE(expectPermissions(user, GlobalPermission::userInput));

    user->setUserRoleIds({role.id});
    ASSERT_TRUE(expectPermissions(user, GlobalPermission::accessAllMedia | GlobalPermission::userInput));

    userRolesManager()->removeUserRole(role.id);
    ASSERT_TRUE(expectPermissions(user, GlobalPermission::userInput));
}

TEST_F(QnCachedGlobalPermissionsManagerTest, checkMultipleRoles)
{
    auto mediaRole = createRole(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(expectPermissions(mediaRole, GlobalPermission::accessAllMedia));

    auto inputRole = createRole(GlobalPermission::userInput);
    ASSERT_TRUE(expectPermissions(inputRole, GlobalPermission::userInput));

    auto user = addUser(GlobalPermission::none);
    ASSERT_TRUE(expectPermissions(user, GlobalPermission::none));

    user->setUserRoleIds({mediaRole.id});
    ASSERT_TRUE(expectPermissions(user, GlobalPermission::accessAllMedia));

    user->setUserRoleIds({inputRole.id});
    ASSERT_TRUE(expectPermissions(user, GlobalPermission::userInput));

    user->setUserRoleIds({mediaRole.id, inputRole.id});
    ASSERT_TRUE(expectPermissions(user, GlobalPermission::accessAllMedia | GlobalPermission::userInput));

    userRolesManager()->removeUserRole(mediaRole.id);
    ASSERT_TRUE(expectPermissions(user, GlobalPermission::userInput));

    userRolesManager()->removeUserRole(inputRole.id);
    ASSERT_TRUE(expectPermissions(user, GlobalPermission::none));
}

TEST_F(QnCachedGlobalPermissionsManagerTest, checkRoleInheritance)
{
    auto mediaRole = createRole(GlobalPermission::accessAllMedia);
    ASSERT_TRUE(expectPermissions(mediaRole, GlobalPermission::accessAllMedia));

    auto inputRole = createRole(GlobalPermission::userInput);
    ASSERT_TRUE(expectPermissions(inputRole, GlobalPermission::userInput));

    auto inheritedRole = createRole(GlobalPermission::none, {mediaRole.id, inputRole.id});
    ASSERT_TRUE(expectPermissions(inheritedRole, GlobalPermission::accessAllMedia | GlobalPermission::userInput));

    auto user = addUser(GlobalPermission::none);
    ASSERT_TRUE(expectPermissions(user, GlobalPermission::none));

    user->setUserRoleIds({inheritedRole.id});
    ASSERT_TRUE(expectPermissions(user, GlobalPermission::accessAllMedia | GlobalPermission::userInput));

    userRolesManager()->removeUserRole(mediaRole.id);
    ASSERT_TRUE(expectPermissions(inheritedRole, GlobalPermission::userInput));
    ASSERT_TRUE(expectPermissions(user, GlobalPermission::userInput));

    userRolesManager()->removeUserRole(inputRole.id);
    ASSERT_TRUE(expectPermissions(inheritedRole, GlobalPermission::none));
    ASSERT_TRUE(expectPermissions(user, GlobalPermission::none));
}

} // namespace nx::vms::common::test
