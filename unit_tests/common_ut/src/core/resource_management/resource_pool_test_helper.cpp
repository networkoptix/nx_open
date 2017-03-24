#include "resource_pool_test_helper.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <test_support/resource/storage_resource_stub.h>
#include <test_support/resource/camera_resource_stub.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/videowall_resource.h>

QString QnResourcePoolTestHelper::kTestUserName = QStringLiteral("user");
QString QnResourcePoolTestHelper::kTestUserName2 = QStringLiteral("user_2");

QnUserResourcePtr QnResourcePoolTestHelper::createUser(Qn::GlobalPermissions globalPermissions,
    const QString& name,
    QnUserType userType)
{
    QnUserResourcePtr user(new QnUserResource(userType));
    user->setId(QnUuid::createUuid());
    user->setName(name);
    user->setRawPermissions(globalPermissions);
    user->addFlags(Qn::remote);
    return user;
}

QnUserResourcePtr QnResourcePoolTestHelper::addUser(Qn::GlobalPermissions globalPermissions,
    const QString& name,
    QnUserType userType)
{
    auto user = createUser(globalPermissions, name, userType);
    qnResPool->addResource(user);
    return user;
}

QnLayoutResourcePtr QnResourcePoolTestHelper::createLayout()
{
    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->setId(QnUuid::createUuid());
    return layout;
}

QnLayoutResourcePtr QnResourcePoolTestHelper::addLayout()
{
    auto layout = createLayout();
    qnResPool->addResource(layout);
    return layout;
}

QnVirtualCameraResourcePtr QnResourcePoolTestHelper::createCamera()
{
    QnVirtualCameraResourcePtr camera(new nx::CameraResourceStub());
    camera->setName(QStringLiteral("camera"));
    return camera;
}

QnVirtualCameraResourcePtr QnResourcePoolTestHelper::addCamera()
{
    auto camera = createCamera();
    qnResPool->addResource(camera);
    return camera;
}

QnWebPageResourcePtr QnResourcePoolTestHelper::addWebPage()
{
    QnWebPageResourcePtr webPage(new QnWebPageResource(QStringLiteral("http://www.ru")));
    qnResPool->addResource(webPage);
    return webPage;
}

QnVideoWallResourcePtr QnResourcePoolTestHelper::createVideoWall()
{
    QnVideoWallResourcePtr videoWall(new QnVideoWallResource());
    videoWall->setId(QnUuid::createUuid());
    return videoWall;
}

QnVideoWallResourcePtr QnResourcePoolTestHelper::addVideoWall()
{
    auto videoWall = createVideoWall();
    qnResPool->addResource(videoWall);
    return videoWall;
}

QnMediaServerResourcePtr QnResourcePoolTestHelper::addServer()
{
    QnMediaServerResourcePtr server(new QnMediaServerResource());
    server->setId(QnUuid::createUuid());
    qnResPool->addResource(server);
    return server;
}

QnStorageResourcePtr QnResourcePoolTestHelper::addStorage(const QnMediaServerResourcePtr& server)
{
    QnStorageResourcePtr storage(new nx::StorageResourceStub());
    storage->setParentId(server->getId());
    qnResPool->addResource(storage);
    return storage;
}

ec2::ApiUserRoleData QnResourcePoolTestHelper::createRole(Qn::GlobalPermissions permissions)
{
    return ec2::ApiUserRoleData(QnUuid::createUuid(), QStringLiteral("test_role"),
        permissions);
}
