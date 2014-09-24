#include "camera_pool.h"

#include "video_camera.h"
#include "core/resource/security_cam_resource.h"

#ifdef Q_OS_WIN
#   include "plugins/storage/dts/vmax480/vmax480_stream_fetcher.h"
#endif

#include <utils/common/app_info.h>

QMutex QnVideoCameraPool::m_staticMtx;

void QnVideoCameraPool::stop()
{
    foreach(QnVideoCamera* camera, m_cameras.values())
        camera->beforeStop();

#if defined(Q_OS_WIN) && defined(ENABLE_VMAX)
        VMaxStreamFetcher::pleaseStopAll(); // increase stop time
#endif
    foreach(QnVideoCamera* camera, m_cameras.values())
        delete camera;

    m_cameras.clear();
}

QnVideoCameraPool::~QnVideoCameraPool()
{
    stop();
}

static QnVideoCameraPool* globalInstance = NULL;

void QnVideoCameraPool::initStaticInstance( QnVideoCameraPool* inst )
{
    globalInstance = inst;
}

QnVideoCameraPool* QnVideoCameraPool::instance()
{
    return globalInstance;
    //QMutexLocker lock(&m_staticMtx);
    //static QnVideoCameraPool inst;
    //return &inst;
}

QnVideoCamera* QnVideoCameraPool::getVideoCamera(const QnResourcePtr& res)
{
    if (!dynamic_cast<const QnSecurityCamResource*>(res.data()))
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

void QnVideoCameraPool::removeVideoCamera(const QnResourcePtr& res)
{
    QMutexLocker lock(&m_staticMtx);
    CameraMap::iterator itr = m_cameras.find(res);
    if (itr == m_cameras.end())
        return;
    delete itr.value();
    m_cameras.erase(itr);
}
