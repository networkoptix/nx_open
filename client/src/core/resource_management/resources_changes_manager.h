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

    typedef std::function<void (const QnVirtualCameraResourcePtr &)> CameraChangesFunction;

    /** Apply changes to the given list of cameras. */
    void saveCameras(const QnVirtualCameraResourceList &cameras, CameraChangesFunction applyChanges);

};

#define qnResourcesChangesManager QnResourcesChangesManager::instance()
