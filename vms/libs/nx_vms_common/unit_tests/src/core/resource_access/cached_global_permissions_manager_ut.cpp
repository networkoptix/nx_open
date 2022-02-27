// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <common/common_module.h>
#include <common/static_common_module.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/core/access/access_types.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>
#include <nx/vms/common/test_support/resource/resource_pool_test_helper.h>

class QnCachedGlobalPermissionsManagerTest: public testing::Test, protected QnResourcePoolTestHelper
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_staticCommon = std::make_unique<QnStaticCommonModule>();
        m_module = std::make_unique<QnCommonModule>(
            /*clientMode*/ false,
            nx::core::access::Mode::cached);
        initializeContext(m_module.get());

        QObject::connect(globalPermissionsManager(),
            &QnGlobalPermissionsManager::globalPermissionsChanged,
            [this](const QnResourceAccessSubject& subject, GlobalPermissions value)
            {
                at_globalPermissionsChanged(subject, value);
            });
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        deinitializeContext();
        ASSERT_TRUE(m_awaitedAccessQueue.empty());
        m_currentUser.clear();
        m_module.reset();
        m_staticCommon.reset();
    }

    bool hasGlobalPermission(const QnResourceAccessSubject& subject,
        GlobalPermission requiredPermission)
    {
        return globalPermissionsManager()->hasGlobalPermission(subject, requiredPermission);
    }

    void awaitPermissions(const QnResourceAccessSubject& subject, GlobalPermissions value)
    {
        m_awaitedAccessQueue.emplace_back(subject, value);
    }

    void at_globalPermissionsChanged(const QnResourceAccessSubject& subject,
        GlobalPermissions value)
    {
        if (m_awaitedAccessQueue.empty())
            return;

        auto awaited = m_awaitedAccessQueue.front();
        if (awaited.subject == subject)
        {
            m_awaitedAccessQueue.pop_front();
            ASSERT_EQ(value, awaited.value);
        }
    }

    std::unique_ptr<QnStaticCommonModule> m_staticCommon;
    std::unique_ptr<QnCommonModule> m_module;
    QnUserResourcePtr m_currentUser;

    struct AwaitedAccess
    {
        AwaitedAccess(const QnResourceAccessSubject& subject, GlobalPermissions value)
            :
            subject(subject),
            value(value)
        {
        }

        QnResourceAccessSubject subject;
        GlobalPermissions value;
    };
    std::deque<AwaitedAccess> m_awaitedAccessQueue;
};

TEST_F(QnCachedGlobalPermissionsManagerTest, checkRoleRemoved)
{
    auto role = createRole(GlobalPermission::accessAllMedia);
    userRolesManager()->addOrUpdateUserRole(role);
    ASSERT_TRUE(hasGlobalPermission(role, GlobalPermission::accessAllMedia));

    auto user = addUser(GlobalPermission::none);
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));

    user->setUserRoleId(role.id);
    ASSERT_TRUE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));

    userRolesManager()->removeUserRole(role.id);
    ASSERT_FALSE(hasGlobalPermission(user, GlobalPermission::accessAllMedia));
}

TEST_F(QnCachedGlobalPermissionsManagerTest, checkRoleRemovedSignalRole)
{
    auto role = createRole(GlobalPermission::accessAllMedia);
    userRolesManager()->addOrUpdateUserRole(role);
    auto user = addUser(GlobalPermission::none);
    user->setUserRoleId(role.id);
    awaitPermissions(role, GlobalPermission::none);
    userRolesManager()->removeUserRole(role.id);
}

TEST_F(QnCachedGlobalPermissionsManagerTest, checkRoleRemovedSignalUser)
{
    auto role = createRole(GlobalPermission::accessAllMedia);
    userRolesManager()->addOrUpdateUserRole(role);
    auto user = addUser(GlobalPermission::none);
    user->setUserRoleId(role.id);
    awaitPermissions(user, GlobalPermission::none);
    userRolesManager()->removeUserRole(role.id);
}
