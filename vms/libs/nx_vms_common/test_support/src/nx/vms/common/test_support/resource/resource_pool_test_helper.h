// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/user_resource.h> //< Not so good but we can allow it for the test module.
#include <nx/vms/common/system_context_aware.h>

#include "camera_resource_stub.h"

namespace nx::vms::api { struct UserRoleData; }

class NX_VMS_COMMON_TEST_SUPPORT_API QnResourcePoolTestHelper:
    public nx::vms::common::SystemContextAware
{
public:
    using nx::vms::common::SystemContextAware::SystemContextAware;

    static constexpr auto kTestUserName = "user";
    static constexpr auto kTestUserName2 = "user_2";

    QnUserResourcePtr createUser(
        GlobalPermissions globalPermissions,
        const QString& name = kTestUserName,
        nx::vms::api::UserType userType = nx::vms::api::UserType::local);

    QnUserResourcePtr addUser(
        GlobalPermissions globalPermissions,
        const QString& name = kTestUserName,
        nx::vms::api::UserType userType = nx::vms::api::UserType::local);

    QnLayoutResourcePtr createLayout();
    QnLayoutResourcePtr addLayout();
    void addToLayout(const QnLayoutResourcePtr& layout, const QnResourcePtr& resource);

    static constexpr auto kUseDefaultLicense = Qn::LC_Count;
    nx::CameraResourceStubPtr createCamera(Qn::LicenseType licenseType = kUseDefaultLicense);
    nx::CameraResourceStubPtr addCamera(Qn::LicenseType licenseType = kUseDefaultLicense);
    std::vector<nx::CameraResourceStubPtr> addCameras(size_t count);

    nx::CameraResourceStubPtr createDesktopCamera(const QnUserResourcePtr& user);
    nx::CameraResourceStubPtr addDesktopCamera(const QnUserResourcePtr& user);

    QnWebPageResourcePtr addWebPage();

    QnVideoWallResourcePtr createVideoWall();
    QnVideoWallResourcePtr addVideoWall();
    QnLayoutResourcePtr addLayoutForVideoWall(const QnVideoWallResourcePtr& videoWall);

    QnMediaServerResourcePtr addServer(nx::vms::api::ServerFlags additionalFlags = nx::vms::api::SF_None);

    QnStorageResourcePtr addStorage(const QnMediaServerResourcePtr& server);

    nx::vms::api::UserRoleData createRole(
        GlobalPermissions permissions, std::vector<QnUuid> parentRoleIds = {});
};
