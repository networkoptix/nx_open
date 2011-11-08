#ifndef __CAMERA_POOL_H__
#define __CAMERA_POOL_H__

#include <QMap>
#include <QMutex>
#include "core/resource/resource.h"
#include "camera/video_camera.h"

#define qnCameraPool QnVideoCameraPool::instance()

class QnVideoCameraPool
{
public:
    static QnVideoCameraPool* instance();
    virtual ~QnVideoCameraPool();

    QnVideoCamera* getVideoCamera(QnResourcePtr res);
    void removeVideoCamera(QnResourcePtr res);
private:
    QnVideoCameraPool();
private:
    typedef QMap<QnResourcePtr, QnVideoCamera*> CameraMap;
    CameraMap m_cameras;
    static QMutex m_staticMtx;
};

#endif //  __CAMERA_POOL_H__
