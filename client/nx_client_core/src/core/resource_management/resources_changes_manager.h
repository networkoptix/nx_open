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

    typedef std::function<void (const QnVirtualCameraResourcePtr &)>    CameraChangesFunction;
    typedef std::function<void (const QnMediaServerResourcePtr &)>      ServerChangesFunction;
    typedef std::function<void (const QnUserResourcePtr &)>             UserChangesFunction;
    typedef std::function<void (const QnVideoWallResourcePtr &)>        VideoWallChangesFunction;
    typedef std::function<void (const QnLayoutResourcePtr &)>           LayoutChangesFunction;
    typedef std::function<void (const QnWebPageResourcePtr &)>          WebPageChangesFunction;


    typedef std::function<void ()> BatchChangesFunction;
    typedef std::function<void ()> RollbackFunction;

    using ActionResultCallback = std::function<void (bool success)>;

    /** Generic function to delete resources. */
    void deleteResources(
        const QnResourceList& resources,
        const ActionResultCallback& callback = ActionResultCallback());

    /** Apply changes to the given camera. */
    void saveCamera(const QnVirtualCameraResourcePtr &camera, CameraChangesFunction applyChanges);

    /** Apply changes to the given list of cameras. */
    void saveCameras(const QnVirtualCameraResourceList &cameras, CameraChangesFunction applyChanges);

    /** Apply changes to the given list of cameras. */
    void saveCamerasBatch(const QnVirtualCameraResourceList &cameras, BatchChangesFunction applyChanges, RollbackFunction rollback = []{});

    /** Apply changes to the given camera as a core resource, e.g. change parent id.
     *  Strongly not recommended to use from client code.
     */
    void saveCamerasCore(const QnVirtualCameraResourceList &cameras, CameraChangesFunction applyChanges);

    /** Apply changes to the given server. */
    void saveServer(const QnMediaServerResourcePtr &server, ServerChangesFunction applyChanges);

    /** Apply changes to the given list of servers. */
    void saveServers(const QnMediaServerResourceList &servers, ServerChangesFunction applyChanges);

    /** Apply changes to the given list of servers. */
    void saveServersBatch(const QnMediaServerResourceList &servers, BatchChangesFunction applyChanges, RollbackFunction rollback = []{});

    /** Apply changes to the given user. */
    void saveUser(const QnUserResourcePtr &user, UserChangesFunction applyChanges,
        const ActionResultCallback& callback = ActionResultCallback());

    /** Save accessible resources for the given user */
    void saveAccessibleResources(const QnResourceAccessSubject& subject,
        const QSet<QnUuid>& accessibleResources);

    void saveUserRole(const ec2::ApiUserRoleData& role);
    void removeUserRole(const QnUuid& id);

    /** Apply changes to the given videoWall. */
    void saveVideoWall(const QnVideoWallResourcePtr &videoWall, VideoWallChangesFunction applyChanges);

    /** Apply changes to the given layout. */
    void saveLayout(const QnLayoutResourcePtr &layout, LayoutChangesFunction applyChanges);

    /** Apply changes to the given web page. */
    void saveWebPage(const QnWebPageResourcePtr &webPage, WebPageChangesFunction applyChanges);

signals:
    /** This signal is emitted every time when changes cannot be saved. */
    void saveChangesFailed(const QnResourceList &resources);

    /** This signal is emitted every time when we cannot delete resources. */
    void resourceDeletingFailed(const QnResourceList &resources);
};

#define qnResourcesChangesManager QnResourcesChangesManager::instance()
