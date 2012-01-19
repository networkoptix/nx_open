#include "camera_pool.h"

#include "video_camera.h"
#include "core/resource/security_cam_resource.h"

QMutex QnVideoCameraPool::m_staticMtx;

QnVideoCameraPool::QnVideoCameraPool()
{
}

QnVideoCameraPool::~QnVideoCameraPool()
{
    foreach(QnVideoCamera* camera, m_cameras.values())
        camera->beforeDisconnectFromResource();
    foreach(QnVideoCamera* camera, m_cameras.values())
        delete camera;
}

QnVideoCameraPool* QnVideoCameraPool::instance()
{
    QMutexLocker lock(&m_staticMtx);
    static QnVideoCameraPool inst;
    return &inst;
}

QnVideoCamera* QnVideoCameraPool::getVideoCamera(QnResourcePtr res)
{
    QnSecurityCamResourcePtr cameraRes = qSharedPointerDynamicCast<QnSecurityCamResource>(res);
    if (!cameraRes)
        return 0;

    QMutexLocker lock(&m_staticMtx);
    QnVideoCamera* result = 0;
    CameraMap::iterator itr = m_cameras.find(res);
    if (itr == m_cameras.end()) {
        result = new QnVideoCamera(res);
        m_cameras.insert(res, result);
        return result;
    }
    else {
        return itr.value();
    }
}

void QnVideoCameraPool::removeVideoCamera(QnResourcePtr res)
{
    QMutexLocker lock(&m_staticMtx);
    CameraMap::iterator itr = m_cameras.find(res);
    if (itr == m_cameras.end())
        return;
    delete itr.value();
    m_cameras.erase(itr);
}
