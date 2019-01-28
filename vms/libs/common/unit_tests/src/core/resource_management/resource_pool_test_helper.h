#pragma once

#include <common/common_globals.h>

#include <common/common_module_aware.h>

#include <nx_ec/data/api_fwd.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/user_resource.h> //not so good but we can allow it for test module
#include <test_support/resource/camera_resource_stub.h>

class QnStaticCommonModule;

class QnResourcePoolTestHelper: public QnCommonModuleAware
{
public:
    QnResourcePoolTestHelper();
    virtual ~QnResourcePoolTestHelper();

    static QString kTestUserName;
    static QString kTestUserName2;

    QnUserResourcePtr createUser(GlobalPermissions globalPermissions,
        const QString& name = kTestUserName,
        QnUserType userType = QnUserType::Local);

    QnUserResourcePtr addUser(GlobalPermissions globalPermissions,
        const QString& name = kTestUserName,
        QnUserType userType = QnUserType::Local);

    QnLayoutResourcePtr createLayout();
    QnLayoutResourcePtr addLayout();

    static constexpr auto kUseDefaultLicense = Qn::LC_Count;
    nx::CameraResourceStubPtr createCamera(Qn::LicenseType licenseType = kUseDefaultLicense);
    nx::CameraResourceStubPtr addCamera(Qn::LicenseType licenseType = kUseDefaultLicense);

    QnWebPageResourcePtr addWebPage();

    QnVideoWallResourcePtr createVideoWall();
    QnVideoWallResourcePtr addVideoWall();
    QnLayoutResourcePtr addLayoutForVideoWall(const QnVideoWallResourcePtr& videoWall);

    QnMediaServerResourcePtr addServer(nx::vms::api::ServerFlags additionalFlags = nx::vms::api::SF_None);

    QnStorageResourcePtr addStorage(const QnMediaServerResourcePtr& server);

    nx::vms::api::UserRoleData createRole(GlobalPermissions permissions);

private:
    QScopedPointer<QnStaticCommonModule> m_staticCommon;

};
