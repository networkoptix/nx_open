// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_map.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>

namespace nx::vms::client::desktop {

/**
 * Utility class for saving resources user attributes.
 * Supports changes rollback in case they cannot be saved on server.
 */
class ResourcesChangesManager: public QObject,
    public core::CommonModuleAware,
    public core::RemoteConnectionAware
{
    Q_OBJECT
    typedef QObject base_type;

public:
    ResourcesChangesManager(QObject* parent = nullptr);
    ~ResourcesChangesManager();

    using CameraChangesFunction = std::function<void(const QnVirtualCameraResourcePtr&)>;
    using ServerChangesFunction = std::function<void(const QnMediaServerResourcePtr&)>;
    using UserChangesFunction = std::function<void(const QnUserResourcePtr&)>;
    using UserCallbackFunction = std::function<void(bool, const QnUserResourcePtr&)>;
    using RoleCallbackFunction = std::function<void(bool, const nx::vms::api::UserRoleData&)>;
    using VideoWallChangesFunction = std::function<void(const QnVideoWallResourcePtr&)>;
    using VideoWallCallbackFunction = std::function<void(bool, const QnVideoWallResourcePtr&)>;
    using LayoutChangesFunction = std::function<void(const QnLayoutResourcePtr&)>;
    using LayoutCallbackFunction = std::function<void(bool, const QnLayoutResourcePtr&)>;
    using WebPageChangesFunction = std::function<void(const QnWebPageResourcePtr&)>;
    using WebPageCallbackFunction = std::function<void(bool, const QnWebPageResourcePtr&)>;
    using AccessRightsCallbackFunction = std::function<void(bool)>;

    using GenericChangesFunction = std::function<void()>;
    using GenericCallbackFunction = std::function<void(bool)>;

    /** Generic function to delete resources. */
    void deleteResources(
        const QnResourceList& resources,
        const GenericCallbackFunction& callback = GenericCallbackFunction());

    /** Apply changes to the given camera. */
    void saveCamera(const QnVirtualCameraResourcePtr& camera,
        CameraChangesFunction applyChanges);

    /** Apply changes to the given list of cameras. */
    void saveCameras(const QnVirtualCameraResourceList& cameras,
        CameraChangesFunction applyChanges);

    /** Apply changes to the given list of cameras. */
    void saveCamerasBatch(const QnVirtualCameraResourceList& cameras,
        GenericChangesFunction applyChanges,
        GenericCallbackFunction callback = GenericCallbackFunction());

    /** Apply changes to the given camera as a core resource, e.g. change parent id.
     *  Strongly not recommended to use from client code.
     */
    void saveCamerasCore(const QnVirtualCameraResourceList& cameras,
        CameraChangesFunction applyChanges);

    /** Apply changes to the given server. */
    void saveServer(const QnMediaServerResourcePtr& server,
        ServerChangesFunction applyChanges);

    /** Apply changes to the given list of servers. */
    void saveServers(const QnMediaServerResourceList& servers,
        ServerChangesFunction applyChanges);

    /** Apply changes to the given list of servers. */
    void saveServersBatch(const QnMediaServerResourceList& servers,
        GenericChangesFunction applyChanges);

    /** Apply changes to the given user. */
    void saveUser(const QnUserResourcePtr& user,
        QnUserResource::DigestSupport digestSupport,
        UserChangesFunction applyChanges,
        UserCallbackFunction callback = UserCallbackFunction());

    /** Apply changes to the given users. */
    void saveUsers(const QnUserResourceList& users,
        QnUserResource::DigestSupport digestSupport = QnUserResource::DigestSupport::keep);

    /** Save accessible resources for the given user. */
    void saveAccessibleResources(
        const QnResourceAccessSubject& subject,
        const QSet<QnUuid>& accessibleResources,
        GlobalPermissions permissions);

    /** Save accessible rights for the given subject. */
    void saveAccessRights(
        const QnResourceAccessSubject& subject,
        const nx::core::access::ResourceAccessMap& accessRights,
        AccessRightsCallbackFunction callback);

    void saveUserRole(const nx::vms::api::UserRoleData& role,
        RoleCallbackFunction callback = RoleCallbackFunction());
    void removeUserRole(const QnUuid& id);

    /** Apply changes to the given videoWall. */
    void saveVideoWall(const QnVideoWallResourcePtr& videoWall,
        VideoWallChangesFunction applyChanges = VideoWallChangesFunction(),
        VideoWallCallbackFunction callback = VideoWallCallbackFunction());

    /** Apply changes to the given web page. */
    void saveWebPage(const QnWebPageResourcePtr& webPage,
        WebPageChangesFunction applyChanges = WebPageChangesFunction(),
        WebPageCallbackFunction callback = WebPageCallbackFunction());

signals:
    /** This signal is emitted every time when changes cannot be saved. */
    void saveChangesFailed(const QnResourceList &resources);

    /** This signal is emitted every time when we cannot delete resources. */
    void resourceDeletingFailed(const QnResourceList &resources);
};

} // namespace nx::vms::client::desktop

#define qnResourcesChangesManager appContext()->resourcesChangesManager()
