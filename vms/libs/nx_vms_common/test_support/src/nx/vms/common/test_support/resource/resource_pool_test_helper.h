// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/user_resource.h> //< Not so good but we can allow it for the test module.
#include <nx/vms/common/resource/camera_resource_stub.h>
#include <nx/vms/common/system_context_aware.h>

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
        Ids(const nx::Uuid& id): data({id}) {}
        Ids(std::initializer_list<nx::Uuid> ids): data(ids) {}

        std::vector<nx::Uuid> data;
    };

    static constexpr auto kTestServerName = "server";
    static constexpr auto kTestUserName = "user";
    static constexpr auto kTestUserName2 = "user_2";

    QnResourcePoolTestHelper(nx::vms::common::SystemContext* context);
    virtual ~QnResourcePoolTestHelper() override;

    static QnUserResourcePtr createUser(
        Ids parentGroupIds,
        const QString& name = kTestUserName,
        nx::vms::api::UserType userType = nx::vms::api::UserType::local,
        GlobalPermissions globalPermissions = GlobalPermission::none,
        const std::map<nx::Uuid, nx::vms::api::AccessRights>& resourceAccessRights = {},
        const QString& ldapDn = "");

    QnUserResourcePtr addUser(const QString& name,
        const std::vector<nx::Uuid>& groupIds = {}) const;

    QnUserResourcePtr addUser(
        Ids parentGroupIds,
        const QString& name = kTestUserName,
        nx::vms::api::UserType userType = nx::vms::api::UserType::local,
        GlobalPermissions globalPermissions = GlobalPermission::none,
        const std::map<nx::Uuid, nx::vms::api::AccessRights>& resourceAccessRights = {},
        const QString& ldapDn = "");

    QnUserResourcePtr addOrgUser(
        Ids orgGroupIds,
        const QString& name,
        GlobalPermissions globalPermissions = GlobalPermission::none,
        const std::map<nx::Uuid, nx::vms::api::AccessRights>& resourceAccessRights = {});

    virtual QnMediaServerResourcePtr createServer(
        const nx::Uuid& id = nx::Uuid::createUuid()) const;

    QnMediaServerResourcePtr addServer(
        const QString& name = kTestServerName,
        const nx::Uuid& id = nx::Uuid::createUuid()) const;

    QnMediaServerResourcePtr addEdgeServer(const QString& name, const QString& address) const;

    virtual QnLayoutResourcePtr createLayout() const;

    QnLayoutResourcePtr addLayout();
    QnLayoutResourcePtr addLayout(
        const QString& name,
        const nx::Uuid& parentId = nx::Uuid()) const;

    nx::Uuid addToLayout(const QnLayoutResourcePtr& layout, const QnResourcePtr& resource);
    void addToLayout(const QnLayoutResourcePtr& layout, const QnResourceList& resources) const;

    static constexpr auto kUseDefaultLicense = Qn::LC_Count;
    static nx::CameraResourceStubPtr createCamera(Qn::LicenseType licenseType = kUseDefaultLicense);
    nx::CameraResourceStubPtr addCamera(Qn::LicenseType licenseType = kUseDefaultLicense);
    std::vector<nx::CameraResourceStubPtr> addCameras(size_t count);

    nx::CameraResourceStubPtr addCamera(
        const QString& name,
        const nx::Uuid& parentId = nx::Uuid(),
        const QString& hostAddress = QString()) const;
    nx::CameraResourceStubPtr addEdgeCamera(
        const QString& name, const QnMediaServerResourcePtr& edgeServer) const;
    nx::CameraResourceStubPtr addVirtualCamera(const QString& name,
        const nx::Uuid& parentId = nx::Uuid()) const;
    nx::CameraResourceStubPtr addIOModule(const QString& name,
        const nx::Uuid& parentId = nx::Uuid()) const;
    nx::CameraResourceStubPtr addRecorderCamera(const QString& name,
        const QString& groupId, const nx::Uuid& parentId = nx::Uuid()) const;
    nx::CameraResourceStubPtr addMultisensorSubCamera(const QString& name,
        const QString& groupId, const nx::Uuid& parentId = nx::Uuid()) const;

    nx::CameraResourceStubPtr createDesktopCamera(const QnUserResourcePtr& user);
    nx::CameraResourceStubPtr addDesktopCamera(const QnUserResourcePtr& user);

    void toIntercom(nx::CameraResourceStubPtr camera);

    nx::CameraResourceStubPtr addIntercomCamera();
    QnLayoutResourcePtr addIntercomLayout(const QnVirtualCameraResourcePtr& intercomCamera);
    QnLayoutResourcePtr addIntercom() { return addIntercomLayout(addIntercomCamera()); }

    QnWebPageResourcePtr addWebPage();

    void setVideoWallScreenRuntimeStatus(
        const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallItem& videoWallScreen,
        bool isOnline,
        const nx::Uuid& controlledBy = nx::Uuid());

    void setupAccessToResourceForUser(
        const QnUserResourcePtr& user,
        const QnResourcePtr& resource,
        bool isAccessible) const;

    void setupAccessToResourceForUser(
        const QnUserResourcePtr& user,
        const QnResourcePtr& resource,
        nx::vms::api::AccessRights accessRights) const;

    void setupAccessToObjectForUser(
        const QnUserResourcePtr& user,
        const nx::Uuid& resourceOrGroupId,
        nx::vms::api::AccessRights accessRights) const;

    void setupAllMediaAccess(
        const QnUserResourcePtr& user, nx::vms::api::AccessRights accessRights) const;

    QnVideoWallResourcePtr createVideoWall();
    QnVideoWallResourcePtr addVideoWall();
    QnLayoutResourcePtr addLayoutForVideoWall(const QnVideoWallResourcePtr& videoWall);
    nx::Uuid addVideoWallItem(const QnVideoWallResourcePtr& videoWall,
        const QnLayoutResourcePtr& itemLayout);
    bool changeVideoWallItem(const QnVideoWallResourcePtr& videoWall, const nx::Uuid& itemId,
        const QnLayoutResourcePtr& itemLayout);

    QnStorageResourcePtr addStorage(const QnMediaServerResourcePtr& server);

    nx::vms::api::UserGroupData createUserGroup(
        QString name,
        Ids parentGroupIds = NoGroup,
        const std::map<nx::Uuid, nx::vms::api::AccessRights>& resourceAccessRights = {},
        GlobalPermissions permissions = GlobalPermission::none);

    void addOrUpdateUserGroup(const nx::vms::api::UserGroupData& group);
    void removeUserGroup(const nx::Uuid& groupId);

    nx::vms::common::AnalyticsPluginResourcePtr addAnalyticsIntegration(
        const nx::vms::api::analytics::EngineManifest& manifest = {});

    void clear();
};
