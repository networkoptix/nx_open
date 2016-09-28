#pragma once

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/user_resource.h> //not so good but we can allow it for test module

class QnResourcePoolTestHelper
{
public:
    static QString kTestUserName;
    static QString kTestUserName2;

    QnUserResourcePtr createUser(Qn::GlobalPermissions globalPermissions,
        const QString& name = kTestUserName,
        QnUserType userType = QnUserType::Local);

    QnUserResourcePtr addUser(Qn::GlobalPermissions globalPermissions,
        const QString& name = kTestUserName,
        QnUserType userType = QnUserType::Local);

    QnLayoutResourcePtr createLayout();

    QnVirtualCameraResourcePtr createCamera();

    QnVirtualCameraResourcePtr addCamera();

    QnWebPageResourcePtr addWebPage();

    QnVideoWallResourcePtr addVideoWall();

    QnMediaServerResourcePtr addServer();

    QnStorageResourcePtr addStorage();


};