#pragma once

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>

class QnResourcePoolTestHelper
{
public:
    static QString kTestUserName;
    static QString kTestUserName2;

    QnUserResourcePtr createUser(Qn::GlobalPermissions globalPermissions,
        const QString& name = kTestUserName);

    QnUserResourcePtr addUser(Qn::GlobalPermissions globalPermissions,
        const QString& name = kTestUserName);

    QnLayoutResourcePtr createLayout();

    QnVirtualCameraResourcePtr createCamera();

    QnVirtualCameraResourcePtr addCamera();

    QnWebPageResourcePtr addWebPage();

    QnVideoWallResourcePtr addVideoWall();

    QnMediaServerResourcePtr addServer();

    QnStorageResourcePtr addStorage();


};