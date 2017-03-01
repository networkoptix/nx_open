#ifndef __CAMERA_POOL_H__
#define __CAMERA_POOL_H__

#include <memory>

#include <QtCore/QMap>

#include <utils/thread/mutex.h>

#include <core/resource/resource_fwd.h>

#include "camera/video_camera.h"

#define qnCameraPool QnVideoCameraPool::instance()

class VideoCameraLocker
{
public:
    VideoCameraLocker(QnVideoCameraPtr);
    virtual ~VideoCameraLocker();

private:
    QnVideoCameraPtr m_camera;

    VideoCameraLocker(const VideoCameraLocker&);
    VideoCameraLocker& operator=(const VideoCameraLocker&);
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
    QnVideoCameraPtr getVideoCamera(const QnResourcePtr& res);
    void removeVideoCamera(const QnResourcePtr& res);
    void updateActivity();

    std::unique_ptr<VideoCameraLocker> getVideoCameraLockerByResourceId(const QnUuid& id);

private:
    typedef QMap<QnResourcePtr, QnVideoCameraPtr> CameraMap;
    CameraMap m_cameras;
    static QnMutex m_staticMtx;
};

#endif //  __CAMERA_POOL_H__
