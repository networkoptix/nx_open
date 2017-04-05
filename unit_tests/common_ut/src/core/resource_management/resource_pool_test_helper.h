#pragma once

#include <common/common_globals.h>

#include <common/common_module_aware.h>

#include <nx_ec/data/api_fwd.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/user_resource.h> //not so good but we can allow it for test module

class QnResourcePoolTestHelper: public QnCommonModuleAware
{
public:
    QnResourcePoolTestHelper();

    static QString kTestUserName;
    static QString kTestUserName2;

    QnUserResourcePtr createUser(Qn::GlobalPermissions globalPermissions,
        const QString& name = kTestUserName,
        QnUserType userType = QnUserType::Local);

    QnUserResourcePtr addUser(Qn::GlobalPermissions globalPermissions,
        const QString& name = kTestUserName,
        QnUserType userType = QnUserType::Local);

    QnLayoutResourcePtr createLayout();
    QnLayoutResourcePtr addLayout();

    QnVirtualCameraResourcePtr createCamera();

    QnVirtualCameraResourcePtr addCamera();

    QnWebPageResourcePtr addWebPage();

    QnVideoWallResourcePtr createVideoWall();
    QnVideoWallResourcePtr addVideoWall();

    QnMediaServerResourcePtr addServer();

    QnStorageResourcePtr addStorage(const QnMediaServerResourcePtr& server);

    ec2::ApiUserRoleData createRole(Qn::GlobalPermissions permissions);
};
