// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/user_resource.h> //< Not so good but we can allow it for the test module.
#include <nx/vms/common/system_context_aware.h>

#include "camera_resource_stub.h"

namespace nx::vms::api { struct UserGroupData; }

class NX_VMS_COMMON_TEST_SUPPORT_API QnResourcePoolTestHelper:
    public nx::vms::common::SystemContextAware
{
public:
    using nx::vms::common::SystemContextAware::SystemContextAware;

    enum NoGroupTag { NoGroup };

    struct Ids
    {
        Ids(NoGroupTag) {}
        Ids(const QnUuid& id): data({id}) {}
        Ids(std::initializer_list<QnUuid> ids): data(ids) {}

        std::vector<QnUuid> data;
    };

    static constexpr auto kTestUserName = "user";
    static constexpr auto kTestUserName2 = "user_2";

    QnUserResourcePtr createUser(
        Ids parentGroupIds,
        const QString& name = kTestUserName,
        nx::vms::api::UserType userType = nx::vms::api::UserType::local,
        GlobalPermissions globalPermissions = GlobalPermission::none,
        const std::map<QnUuid, nx::vms::api::AccessRights>& resourceAccessRights = {},
        const QString& ldapDn = "");

    QnUserResourcePtr addUser(
        Ids parentGroupIds,
        const QString& name = kTestUserName,
        nx::vms::api::UserType userType = nx::vms::api::UserType::local,
        GlobalPermissions globalPermissions = GlobalPermission::none,
        const std::map<QnUuid, nx::vms::api::AccessRights>& resourceAccessRights = {},
        const QString& ldapDn = "");

    virtual QnLayoutResourcePtr createLayout();
    QnLayoutResourcePtr addLayout();
    QnUuid addToLayout(const QnLayoutResourcePtr& layout, const QnResourcePtr& resource);

    static constexpr auto kUseDefaultLicense = Qn::LC_Count;
    nx::CameraResourceStubPtr createCamera(Qn::LicenseType licenseType = kUseDefaultLicense);
    nx::CameraResourceStubPtr addCamera(Qn::LicenseType licenseType = kUseDefaultLicense);
    std::vector<nx::CameraResourceStubPtr> addCameras(size_t count);

    nx::CameraResourceStubPtr createDesktopCamera(const QnUserResourcePtr& user);
    nx::CameraResourceStubPtr addDesktopCamera(const QnUserResourcePtr& user);

    nx::CameraResourceStubPtr addIntercomCamera();
    QnLayoutResourcePtr addIntercomLayout(const QnVirtualCameraResourcePtr& intercomCamera);
    QnLayoutResourcePtr addIntercom() { return addIntercomLayout(addIntercomCamera()); }

    QnWebPageResourcePtr addWebPage();

    QnVideoWallResourcePtr createVideoWall();
    QnVideoWallResourcePtr addVideoWall();
    QnLayoutResourcePtr addLayoutForVideoWall(const QnVideoWallResourcePtr& videoWall);
    QnUuid addVideoWallItem(const QnVideoWallResourcePtr& videoWall,
        const QnLayoutResourcePtr& itemLayout);

    QnMediaServerResourcePtr addServer(nx::vms::api::ServerFlags additionalFlags = nx::vms::api::SF_None);

    QnStorageResourcePtr addStorage(const QnMediaServerResourcePtr& server);

    nx::vms::api::UserGroupData createUserGroup(
        QString name,
        Ids parentGroupIds = NoGroup,
        const std::map<QnUuid, nx::vms::api::AccessRights>& resourceAccessRights = {},
        GlobalPermissions permissions = GlobalPermission::none);

    void addOrUpdateUserGroup(const nx::vms::api::UserGroupData& group);
    void removeUserGroup(const QnUuid& groupId);

    void clear();
};
