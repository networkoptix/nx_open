#pragma once

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>
#include <utils/common/singleton.h>

/** 
 * Utility class for saving resources user attributes.
 * Supports changes rollback in case they cannot be saved on server.
 */
class QnResourcesChangesManager: public Connective<QObject>, public Singleton<QnResourcesChangesManager> {
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnResourcesChangesManager(QObject* parent = nullptr);
    ~QnResourcesChangesManager();

    typedef std::function<void (const QnVirtualCameraResourcePtr &)>    CameraChangesFunction;
    typedef std::function<void (const QnMediaServerResourcePtr &)>      ServerChangesFunction;
    typedef std::function<void (const QnUserResourcePtr &)>             UserChangesFunction;
    typedef std::function<void (const QnVideoWallResourcePtr &)>        VideoWallChangesFunction;
    

    typedef std::function<void ()> BatchChangesFunction;
    typedef std::function<void ()> RollbackFunction;

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
    void saveUser(const QnUserResourcePtr &user, UserChangesFunction applyChanges);


    /** Apply changes to the given videoWall. */
    void saveVideoWall(const QnVideoWallResourcePtr &videoWall, VideoWallChangesFunction applyChanges);

signals:
    /** This signal is emitted every time when changes cannot be saved. */
    void saveChangesFailed(const QnResourceList &resources);
};

#define qnResourcesChangesManager QnResourcesChangesManager::instance()
