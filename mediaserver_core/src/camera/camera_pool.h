#ifndef __CAMERA_POOL_H__
#define __CAMERA_POOL_H__

#include <QtCore/QMap>
#include <QtCore/QMutex>

#include <core/resource/resource_fwd.h>
#include "camera/video_camera.h"

#define qnCameraPool QnVideoCameraPool::instance()

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
private:
    typedef QMap<QnResourcePtr, QnVideoCameraPtr> CameraMap;
    CameraMap m_cameras;
    static QMutex m_staticMtx;
};

#endif //  __CAMERA_POOL_H__
