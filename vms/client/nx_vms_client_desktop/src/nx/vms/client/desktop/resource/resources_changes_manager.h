// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_map.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/vms/utils/abstract_session_token_helper.h>

namespace nx::vms::client::desktop {

class SystemContext;

/**
 * Utility class for saving resources user attributes.
 * Supports changes rollback in case they cannot be saved on server.
 */
class ResourcesChangesManager: public QObject
{
    Q_OBJECT
    typedef QObject base_type;

public:
    ResourcesChangesManager(QObject* parent = nullptr);
    ~ResourcesChangesManager();

    using CameraChangesFunction = std::function<void(const QnVirtualCameraResourcePtr&)>;
    using UserChangesFunction = std::function<void(const QnUserResourcePtr&)>;
    using UserCallbackFunction = std::function<void(bool, const QnUserResourcePtr&)>;
    using RoleCallbackFunction = std::function<void(bool, const nx::vms::api::UserGroupData&)>;
    using VideoWallCallbackFunction = std::function<void(bool, const QnVideoWallResourcePtr&)>;
    using LayoutChangesFunction = std::function<void(const QnLayoutResourcePtr&)>;
    using LayoutCallbackFunction = std::function<void(bool, const QnLayoutResourcePtr&)>;
    using WebPageCallbackFunction = std::function<void(bool, const QnWebPageResourcePtr&)>;
    using AccessRightsCallbackFunction = std::function<void(bool)>;

    using GenericChangesFunction = std::function<void()>;
    using GenericCallbackFunction = std::function<void(bool)>;
    using GenericCallbackWithErrorFunction = std::function<void(bool, const QString&)>;
    using DeleteResourceCallbackFunction = std::function<void(bool, const QnResourcePtr&)>;

    /** Generic function to delete resources. */
    void deleteResource(const QnResourcePtr& resource,
        const DeleteResourceCallbackFunction& callback = DeleteResourceCallbackFunction(),
        const nx::vms::common::SessionTokenHelperPtr& helper = nullptr);

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

    using SaveServerCallback = std::function<void(
        bool /*success*/,
        rest::Handle /*requestId*/)>;
    /** Save changes to the given server. */
    rest::Handle saveServer(const QnMediaServerResourcePtr& server, SaveServerCallback callback);

    /* User resource management is handled by UserGroupRequestChain class. */

    /** Save Video Wall. */
    void saveVideoWall(const QnVideoWallResourcePtr& videoWall,
        VideoWallCallbackFunction callback = {});

    /** Save Web Page. */
    void saveWebPage(const QnWebPageResourcePtr& webPage, WebPageCallbackFunction callback = {});

signals:
    /** This signal is emitted every time when changes cannot be saved. */
    void saveChangesFailed(const QnResourceList &resources);

    /** This signal is emitted every time when we cannot delete resources. */
    void resourceDeletingFailed(const QnResourceList &resources);
};

} // namespace nx::vms::client::desktop

#define qnResourcesChangesManager appContext()->resourcesChangesManager()
