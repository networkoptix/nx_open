#include "resource_pool_test_helper.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource_stub.h>
#include <core/resource/camera_resource_stub.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/videowall_resource.h>

QString QnResourcePoolTestHelper::kTestUserName = QStringLiteral("unit_test_user");
QString QnResourcePoolTestHelper::kTestUserName2 = QStringLiteral("unit_test_user_2");

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
    QnVirtualCameraResourcePtr camera(new QnCameraResourceStub());
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

QnVideoWallResourcePtr QnResourcePoolTestHelper::addVideoWall()
{
    QnVideoWallResourcePtr videoWall(new QnVideoWallResource());
    videoWall->setId(QnUuid::createUuid());
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

QnStorageResourcePtr QnResourcePoolTestHelper::addStorage()
{
    QnStorageResourcePtr storage(new QnStorageResourceStub());
    qnResPool->addResource(storage);
    return storage;
}
