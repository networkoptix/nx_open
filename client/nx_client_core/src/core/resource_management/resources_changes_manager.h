#pragma once

#include <client_core/connection_context_aware.h>

#include <core/resource_access/resource_access_subject.h>
#include <core/resource/resource_fwd.h>

#include <utils/common/connective.h>
#include <nx_ec/data/api_fwd.h>

#include <nx/utils/singleton.h>

/**
 * Utility class for saving resources user attributes.
 * Supports changes rollback in case they cannot be saved on server.
 */
class QnResourcesChangesManager: public Connective<QObject>,
    public Singleton<QnResourcesChangesManager>,
    public QnConnectionContextAware
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnResourcesChangesManager(QObject* parent = nullptr);
    ~QnResourcesChangesManager();

    using CameraChangesFunction = std::function<void(const QnVirtualCameraResourcePtr&)>;
    using ServerChangesFunction = std::function<void(const QnMediaServerResourcePtr&)>;
    using UserChangesFunction = std::function<void(const QnUserResourcePtr&)>;
    using UserCallbackFunction = std::function<void(bool, const QnUserResourcePtr&)>;
    using VideoWallChangesFunction = std::function<void(const QnVideoWallResourcePtr&)>;
    using VideoWallCallbackFunction = std::function<void(bool, const QnVideoWallResourcePtr&)>;
    using LayoutChangesFunction = std::function<void(const QnLayoutResourcePtr&)>;
    using LayoutCallbackFunction = std::function<void(bool, const QnLayoutResourcePtr&)>;
    using WebPageChangesFunction = std::function<void(const QnWebPageResourcePtr&)>;
    using WebPageCallbackFunction = std::function<void(bool, const QnWebPageResourcePtr&)>;

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
    void saveUser(const QnUserResourcePtr &user,
        UserChangesFunction applyChanges,
        UserCallbackFunction callback = UserCallbackFunction());

    /** Apply changes to the given users. */
    void saveUsers(const QnUserResourceList& users);

    /** Save accessible resources for the given user */
    void saveAccessibleResources(const QnResourceAccessSubject& subject,
        const QSet<QnUuid>& accessibleResources);

    /** Clean accessible resources for the given user */
    void cleanAccessibleResources(const QnUuid& subject);

    void saveUserRole(const ec2::ApiUserRoleData& role);
    void removeUserRole(const QnUuid& id);

    /** Apply changes to the given videoWall. */
    void saveVideoWall(const QnVideoWallResourcePtr& videoWall,
        VideoWallChangesFunction applyChanges = VideoWallChangesFunction(),
        VideoWallCallbackFunction callback = VideoWallCallbackFunction());

    /** Apply changes to the given layout. */
    void saveLayout(const QnLayoutResourcePtr& layout,
        LayoutChangesFunction applyChanges,
        LayoutCallbackFunction callback = LayoutCallbackFunction());

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

#define qnResourcesChangesManager QnResourcesChangesManager::instance()
