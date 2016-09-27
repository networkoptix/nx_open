#include <gtest/gtest.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource_stub.h>

#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/providers/permissions_resource_access_provider.h>

namespace {
const QString kUserName = QStringLiteral("unit_test_user");
}

class QnPermissionsResourceAccessProviderTest: public testing::Test
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_module.reset(new QnCommonModule());
        m_accessProvider.reset(new QnPermissionsResourceAccessProvider());
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        m_accessProvider.clear();
        m_module.clear();
    }

    QnUserResourcePtr addUser(Qn::GlobalPermissions globalPermissions)
    {
        QnUserResourcePtr user(new QnUserResource(QnUserType::Local));
        user->setId(QnUuid::createUuid());
        user->setName(kUserName);
        user->setRawPermissions(globalPermissions);
        user->addFlags(Qn::remote);
        qnResPool->addResource(user);

        return user;
    }

    QnLayoutResourcePtr createLayout()
    {
        QnLayoutResourcePtr layout(new QnLayoutResource());
        layout->setId(QnUuid::createUuid());
        qnResPool->addResource(layout);

        return layout;
    }

    QnVirtualCameraResourcePtr createCamera()
    {
        QnVirtualCameraResourcePtr camera(new QnCameraResourceStub());
        qnResPool->addResource(camera);

        return camera;
    }

    // Declares the variables your tests want to use.
    QSharedPointer<QnCommonModule> m_module;
    QSharedPointer<QnPermissionsResourceAccessProvider> m_accessProvider;
};

TEST_F(QnPermissionsResourceAccessProviderTest, checkInvalidAccess)
{
    auto camera = createCamera();

    ec2::ApiUserGroupData role;
    QnResourceAccessSubject subject(role);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(m_accessProvider->hasAccess(subject, camera));
}

