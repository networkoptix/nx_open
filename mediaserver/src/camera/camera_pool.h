#ifndef __CAMERA_POOL_H__
#define __CAMERA_POOL_H__

#include <QtCore/QMap>
#include <utils/common/mutex.h>

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
    QnVideoCamera* getVideoCamera(const QnResourcePtr& res);
    void removeVideoCamera(const QnResourcePtr& res);

private:
    typedef QMap<QnResourcePtr, QnVideoCamera*> CameraMap;
    CameraMap m_cameras;
    static QnMutex m_staticMtx;
};

#endif //  __CAMERA_POOL_H__
