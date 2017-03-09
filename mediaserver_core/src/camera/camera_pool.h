#ifndef __CAMERA_POOL_H__
#define __CAMERA_POOL_H__

#include <memory>

#include <QtCore/QMap>

#include <nx/utils/thread/mutex.h>

#include <core/resource/resource_fwd.h>

#include "camera/video_camera.h"

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

class QnVideoCameraPool
{
public:
    static void initStaticInstance( QnVideoCameraPool* inst );
    static QnVideoCameraPool* instance();

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
    static QnMutex m_staticMtx;
};

#endif //  __CAMERA_POOL_H__
