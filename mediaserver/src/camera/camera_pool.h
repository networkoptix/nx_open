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
    static void initStaticInstance( QnVideoCameraPool* inst );
    static QnVideoCameraPool* instance();

    virtual ~QnVideoCameraPool();

    void stop();

    /*!
        \return Object belongs to this pool
    */
    QnVideoCamera* getVideoCamera(QnResourcePtr res);
    void removeVideoCamera(QnResourcePtr res);

private:
    typedef QMap<QnResourcePtr, QnVideoCamera*> CameraMap;
    CameraMap m_cameras;
    static QMutex m_staticMtx;
};

#endif //  __CAMERA_POOL_H__
