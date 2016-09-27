#include <gtest/gtest.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
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
const QString kUserName = QStringLiteral("unit_test_user");
const QString kUserName2 = QStringLiteral("unit_test_user_2");
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

    QnUserResourcePtr addUser(Qn::GlobalPermissions globalPermissions,
        const QString& name = kUserName)
    {
        QnUserResourcePtr user(new QnUserResource(QnUserType::Local));
        user->setId(QnUuid::createUuid());
        user->setName(name);
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

    QnWebPageResourcePtr createWebPage()
    {
        QnWebPageResourcePtr webPage(new QnWebPageResource(QStringLiteral("http://www.ru")));
        qnResPool->addResource(webPage);
        return webPage;
    }

    QnVideoWallResourcePtr createVideoWall()
    {
        QnVideoWallResourcePtr videoWall(new QnVideoWallResource());
        videoWall->setId(QnUuid::createUuid());
        qnResPool->addResource(videoWall);
        return videoWall;
    }

    QnMediaServerResourcePtr createServer()
    {
        QnMediaServerResourcePtr server(new QnMediaServerResource());
        server->setId(QnUuid::createUuid());
        qnResPool->addResource(server);
        return server;
    }

    QnStorageResourcePtr createStorage()
    {
        QnStorageResourcePtr storage(new QnStorageResourceStub());
        qnResPool->addResource(storage);
        return storage;
    }

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

TEST_F(QnPermissionsResourceAccessProviderTest, checkAccessToInvalidResource)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_FALSE(m_accessProvider->hasAccess(user, QnResourcePtr()));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkAdminAccessCamera)
{
    auto camera = createCamera();
    auto user = addUser(Qn::GlobalAdminPermission);
    ASSERT_TRUE(m_accessProvider->hasAccess(user, camera));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkAdminAccessUser)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = addUser(Qn::GlobalAdminPermission, kUserName2);
    ASSERT_TRUE(m_accessProvider->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkAdminAccessLayout)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = createLayout();
    ASSERT_TRUE(m_accessProvider->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkAdminAccessWebPage)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = createWebPage();
    ASSERT_TRUE(m_accessProvider->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkAdminAccessVideoWall)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = createVideoWall();
    ASSERT_TRUE(m_accessProvider->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkAdminAccessServer)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = createServer();
    ASSERT_TRUE(m_accessProvider->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkAdminAccessStorage)
{
    auto user = addUser(Qn::GlobalAdminPermission);
    auto target = createStorage();
    ASSERT_TRUE(m_accessProvider->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkDesktopCameraAccessByName)
{
    auto camera = createCamera();
    camera->setFlags(Qn::desktop_camera);
    camera->setName(kUserName2);

    auto user = addUser(Qn::GlobalAdminPermission);
    auto user2 = addUser(Qn::GlobalAdminPermission, kUserName2);

    ASSERT_FALSE(m_accessProvider->hasAccess(user, camera));
    ASSERT_TRUE(m_accessProvider->hasAccess(user2, camera));
}


TEST_F(QnPermissionsResourceAccessProviderTest, checkDesktopCameraAccessByPermissions)
{
    auto camera = createCamera();
    camera->setFlags(Qn::desktop_camera);
    camera->setName(kUserName);

    auto user = addUser(Qn::GlobalControlVideoWallPermission);
    ASSERT_TRUE(m_accessProvider->hasAccess(user, camera));

    user->setRawPermissions(Qn::GlobalAccessAllMediaPermission);
    ASSERT_FALSE(m_accessProvider->hasAccess(user, camera));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkCameraAccess)
{
    auto target = createCamera();

    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    ASSERT_TRUE(m_accessProvider->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalEditCamerasPermission);
    ASSERT_FALSE(m_accessProvider->hasAccess(user, target));
}

TEST_F(QnPermissionsResourceAccessProviderTest, checkWebPageAccess)
{
    auto target = createWebPage();

    auto user = addUser(Qn::GlobalAccessAllMediaPermission);
    ASSERT_TRUE(m_accessProvider->hasAccess(user, target));

    user->setRawPermissions(Qn::GlobalEditCamerasPermission);
    ASSERT_FALSE(m_accessProvider->hasAccess(user, target));
}