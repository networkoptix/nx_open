#include <gtest/gtest.h>

#include <common/common_module.h>

#include <nx/core/access/access_types.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_pool_test_helper.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource_access/global_permissions_manager.h>

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <test_support/resource/camera_resource_stub.h>

#include <nx/fusion/model_functions.h>

class QnDirectGlobalPermissionsManagerTest: public testing::Test, protected QnResourcePoolTestHelper
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_module.reset(new QnCommonModule(false, nx::core::access::Mode::direct));
        initializeContext(m_module.data());
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        deinitializeContext();
        m_currentUser.clear();
        m_module.clear();
    }

    bool hasGlobalPermission(const QnResourceAccessSubject& subject,
        Qn::GlobalPermission requiredPermission)
    {
        return globalPermissionsManager()->hasGlobalPermission(subject, requiredPermission);
    }

    QSharedPointer<QnCommonModule> m_module;
    QnUserResourcePtr m_currentUser;
};

TEST_F(QnDirectGlobalPermissionsManagerTest, checkRoleRemoved)
{
    auto role = createRole(Qn::GlobalAccessAllMediaPermission);
    userRolesManager()->addOrUpdateUserRole(role);
    ASSERT_TRUE(hasGlobalPermission(role, Qn::GlobalAccessAllMediaPermission));

    auto user = addUser(Qn::NoGlobalPermissions);
    ASSERT_FALSE(hasGlobalPermission(user, Qn::GlobalAccessAllMediaPermission));

    user->setUserRoleId(role.id);
    ASSERT_TRUE(hasGlobalPermission(user, Qn::GlobalAccessAllMediaPermission));

    userRolesManager()->removeUserRole(role.id);
    ASSERT_FALSE(hasGlobalPermission(user, Qn::GlobalAccessAllMediaPermission));
}
