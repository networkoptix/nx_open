#include <gtest/gtest.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_pool_test_helper.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource_stub.h>
#include <core/resource/camera_resource_stub.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/videowall_resource.h>

#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/providers/permissions_resource_access_provider.h>

namespace {

}

class QnPermissionsResourceAccessProviderTest: public testing::Test,
    protected QnResourcePoolTestHelper
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_module.reset(new QnCommonModule());
        m_accessProvider.reset(new QnPermissionsResourceAccessProvider());
        QObject::connect(accessProvider(),
            &QnPermissionsResourceAccessProvider::accessChanged,
            [this](const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
                bool value)
            {
                at_accessChanged(subject, resource, value);
            });
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        ASSERT_TRUE(m_awaitedAccessQueue.empty());
        m_accessProvider.clear();
        m_module.clear();
    }

    QnAbstractResourceAccessProvider* accessProvider() const
    {
        return m_accessProvider.data();
    }



    void awaitAccess(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
        bool value = true)
    {
        m_awaitedAccessQueue.emplace_back(subject, resource, value);
    }

    void at_accessChanged(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
        bool value)
    {
        ASSERT_EQ(value, accessProvider()->hasAccess(subject, resource));

        if (m_awaitedAccessQueue.empty())
            return;

        auto awaited = m_awaitedAccessQueue.front();
        if (awaited.subject == subject && awaited.resource == resource)
        {
            m_awaitedAccessQueue.pop_front();
            ASSERT_EQ(value, awaited.value);
        }
    }

private:
    QSharedPointer<QnCommonModule> m_module;
    QSharedPointer<QnPermissionsResourceAccessProvider> m_accessProvider;

    struct AwaitedAccess
    {
        AwaitedAccess(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
            bool value)
            :
            subject(subject),
            resource(resource),
            value(value)
        {
        }

        QnResourceAccessSubject subject;
        QnResourcePtr resource;
        bool value;
    };
    std::deque<AwaitedAccess> m_awaitedAccessQueue;
};

TEST_F(QnPermissionsResourceAccessProviderTest, checkInvalidAccess)
{
    auto camera = addCamera();

    ec2::ApiUserGroupData role;
    QnResourceAccessSubject subject(role);
    ASSERT_FALSE(subject.isValid());
    ASSERT_FALSE(accessProvider()->hasAccess(subject, camera));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, QnResourcePtr()));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkAdminAccessCamera)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, camera));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkAdminAccessUser)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = addUser(Qn::GlobalAdminPermission, QnResourcePoolTestHelper::kTestUserName2);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkAdminAccessLayout)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = createLayout();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkAdminAccessWebPage)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = addWebPage();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkAdminAccessVideoWall)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = addVideoWall();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkAdminAccessServer)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = addServer();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkAdminAccessStorage)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = addStorage();
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkDesktopCameraAccessByName)
{
    auto camera = addCamera();
    camera->setFlags(Qn::desktop_camera);
    camera->setName(QnResourcePoolTestHelper::kTestUserName2);

    auto user = addUser(Qn::GlobalAdminPermission);
    auto user2 = addUser(Qn::GlobalAdminPermission, QnResourcePoolTestHelper::kTestUserName2);

    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
    ASSERT_TRUE(accessProvider()->hasAccess(user2, camera));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkDesktopCameraAccessByPermissions)
{
    auto camera = addCamera();
    camera->setFlags(Qn::desktop_camera);
    camera->setName(QnResourcePoolTestHelper::kTestUserName);

    auto user = addUser(Qn::GlobalControlVideoWallPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, camera));

    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkCameraAccess)
{
    auto target = addCamera();

    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalEditCamerasPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkWebPageAccess)
{
    auto target = addWebPage();

    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalEditCamerasPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkServerAccess)
{
    auto target = addServer();

    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkStorageAccess)
{
    auto target = addStorage();

    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkLayoutAccess)
{
    auto target = createLayout();

    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkUserAccess)
{
    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    auto target = addUser(Qn::GlobalAccessAllMediaPermission, QnResourcePoolTestHelper::kTestUserName2);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
    ASSERT_TRUE(accessProvider()->hasAccess(user, user));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkVideoWallAccess)
{
    auto target = addVideoWall();

    auto user = addUser(Qn::GlobalControlVideoWallPermission);
    ASSERT_TRUE(accessProvider()->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);
    ASSERT_FALSE(accessProvider()->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, nonPoolResourceAccess)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    QnVirtualCameraResourcePtr camera(new QnCameraResourceStub());
    ASSERT_FALSE(accessProvider()->hasAccess(user, camera));
}

TEST_F(QnPermissionsResourceAccessProviderTest, awaitNewCameraAccess)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto camera = createCamera();
    awaitAccess(user, camera);
    qnResPool->addResource(camera);
}

TEST_F(QnPermissionsResourceAccessProviderTest, awaitRemovedCameraAccess)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto camera = addCamera();
    awaitAccess(user, camera, false);
    qnResPool->removeResource(camera);
}

TEST_F(QnPermissionsResourceAccessProviderTest, awaitNewUserAccess)
{
    auto camera = addCamera();
    auto user = createUser(Qn::GlobalAdminPermission);
    awaitAccess(user, camera);
    qnResPool->addResource(user);
}

TEST_F(QnPermissionsResourceAccessProviderTest, awaitRemovedUserAccess)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    awaitAccess(user, camera, false);
    qnResPool->removeResource(user);
}

TEST_F(QnPermissionsResourceAccessProviderTest, awaitPermissionsChangedAccess)
{
    auto camera = addCamera();
    auto user = addUser(Qn::NoGlobalPermissions);
    awaitAccess(user, camera);
    user->setRawPermissions(Qn::GlobalAdminPermission);
}

TEST_F(QnPermissionsResourceAccessProviderTest, suppressDuplicatedSignals)
{
    auto camera = addCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    awaitAccess(user, camera, false);

    /* Here we should NOT receive the signal */
    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);

    /* Here we should receive the 'false' value. */
    qnResPool->removeResource(user);
}