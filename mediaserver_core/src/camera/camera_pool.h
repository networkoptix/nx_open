#ifndef __CAMERA_POOL_H__
#define __CAMERA_POOL_H__

#include <memory>

#include <QtCore/QMap>

#include <nx/utils/thread/mutex.h>

#include <core/resource/resource_fwd.h>

#include <common/common_module_aware.h>
#include <nx/utils/singleton.h>

#include "camera_fwd.h"

#define qnCameraPool QnVideoCameraPool::instance()

class VideoCameraLocker
{
public:
    VideoCameraLocker(QnVideoCameraPtr);
    virtual ~VideoCameraLocker();

    VideoCameraLocker(const VideoCameraLocker&) = delete;
    VideoCameraLocker& operator=(const VideoCameraLocker&) = delete;

private:
    QnVideoCameraPtr m_camera;
};

class QnVideoCameraPool: public QnCommonModuleAware, public Singleton<QnVideoCameraPool>
{
public:
    QnVideoCameraPool(QnCommonModule* commonModule);
    virtual ~QnVideoCameraPool();

    void stop();

    /*!
        \return Object belongs to this pool
    */
    QnVideoCameraPtr getVideoCamera(const QnResourcePtr& res) const;
    QnVideoCameraPtr addVideoCamera(const QnResourcePtr& res);
    void removeVideoCamera(const QnResourcePtr& res);
    void updateActivity();

    std::unique_ptr<VideoCameraLocker> getVideoCameraLockerByResourceId(const QnUuid& id) const;

private:
    typedef QMap<QnResourcePtr, QnVideoCameraPtr> CameraMap;
    CameraMap m_cameras;
    mutable QnMutex m_mutex;
};

#endif //  __CAMERA_POOL_H__
