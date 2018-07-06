#include "resource_pool_test_helper.h"

#include <common/static_common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <test_support/resource/storage_resource_stub.h>
#include <core/resource/webpage_resource.h>
#include <core/resource/videowall_resource.h>

QString QnResourcePoolTestHelper::kTestUserName = QStringLiteral("user");
QString QnResourcePoolTestHelper::kTestUserName2 = QStringLiteral("user_2");

QnResourcePoolTestHelper::QnResourcePoolTestHelper():
    QnCommonModuleAware(nullptr, true),
    m_staticCommon(new QnStaticCommonModule())
{

}

QnResourcePoolTestHelper::~QnResourcePoolTestHelper()
{
}

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
    resourcePool()->addResource(user);
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
    resourcePool()->addResource(layout);
    return layout;
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::createCamera(Qn::LicenseType licenseType)
{
    nx::CameraResourceStubPtr camera(new nx::CameraResourceStub(licenseType));
    camera->setName(QStringLiteral("camera"));
    return camera;
}

nx::CameraResourceStubPtr QnResourcePoolTestHelper::addCamera(Qn::LicenseType licenseType)
{
    auto camera = createCamera(licenseType);
    resourcePool()->addResource(camera);
    return camera;
}

QnWebPageResourcePtr QnResourcePoolTestHelper::addWebPage()
{
    QnWebPageResourcePtr webPage(new QnWebPageResource(QStringLiteral("http://www.ru")));
    resourcePool()->addResource(webPage);
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
    resourcePool()->addResource(videoWall);
    return videoWall;
}

QnLayoutResourcePtr QnResourcePoolTestHelper::addLayoutForVideoWall(
    const QnVideoWallResourcePtr& videoWall)
{
    auto layout = createLayout();
    layout->setParentId(videoWall->getId());
    resourcePool()->addResource(layout);

    QnVideoWallItem vwitem;
    vwitem.layout = layout->getId();
    videoWall->items()->addItem(vwitem);

    return layout;
}

QnMediaServerResourcePtr QnResourcePoolTestHelper::addServer()
{
    QnMediaServerResourcePtr server(new QnMediaServerResource(commonModule()));
    server->setId(QnUuid::createUuid());
    resourcePool()->addResource(server);
    return server;
}

QnStorageResourcePtr QnResourcePoolTestHelper::addStorage(const QnMediaServerResourcePtr& server)
{
    QnStorageResourcePtr storage(new nx::StorageResourceStub());
    storage->setParentId(server->getId());
    resourcePool()->addResource(storage);
    return storage;
}

ec2::ApiUserRoleData QnResourcePoolTestHelper::createRole(Qn::GlobalPermissions permissions)
{
    return ec2::ApiUserRoleData(QnUuid::createUuid(), QStringLiteral("test_role"),
        permissions);
}
