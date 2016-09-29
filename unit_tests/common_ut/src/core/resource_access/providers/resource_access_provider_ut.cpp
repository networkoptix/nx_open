#include <core/resource_access/providers/access_provider_test_fixture.h>

#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/providers/direct_resource_access_provider.h>
#include <core/resource_access/providers/permissions_resource_access_provider.h>
#include <core/resource_access/shared_resources_manager.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource_access/resource_access_manager.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource_stub.h>
#include <core/resource/camera_resource_stub.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/videowall_resource.h>


class QnResourceAccessProviderTest: public QnAccessProviderTestFixture
{
protected:
    virtual void SetUp()
    {
        QnAccessProviderTestFixture::SetUp();
        accessProvider()->addBaseProvider(new QnPermissionsResourceAccessProvider());
        accessProvider()->addBaseProvider(new QnDirectResourceAccessProvider());
    }

    virtual QnAbstractResourceAccessProvider* createAccessProvider() const override
    {
        return new QnResourceAccessProvider();
    }

    QnResourceAccessProvider* accessProvider() const
    {
        return dynamic_cast<QnResourceAccessProvider*>(QnAccessProviderTestFixture::accessProvider());
    }
};

TEST_F(QnResourceAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    ec2::ApiUserGroupData role;
    QnResourceAccessSubject subject(role);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(QnResourceAccessProviderTest, checkAccessThroughChild)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, camera));
}

TEST_F(QnResourceAccessProviderTest, checkAccessThroughSecondChild)
{
    auto camera = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    ASSERT_TRUE(accessProvider()->hasAccess(user, camera));
}

TEST_F(QnResourceAccessProviderTest, checkAccessThroughBothChildren)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    qnSharedResourcesManager->setSharedResources(user, QSet<QnUuid>() << camera->getId());
    ASSERT_TRUE(accessProvider()->hasAccess(user, camera));
}
