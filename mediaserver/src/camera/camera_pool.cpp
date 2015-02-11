#include "camera_pool.h"

#include "video_camera.h"

#include <core/resource/resource.h>
#include <core/resource/security_cam_resource.h>

#ifdef Q_OS_WIN
#   include "plugins/storage/dts/vmax480/vmax480_stream_fetcher.h"
#endif

#include <utils/common/app_info.h>

QnMutex QnVideoCameraPool::m_staticMtx;

void QnVideoCameraPool::stop()
{
    for(QnVideoCamera* camera: m_cameras.values())
        camera->beforeStop();

#if defined(Q_OS_WIN) && defined(ENABLE_VMAX)
        VMaxStreamFetcher::pleaseStopAll(); // increase stop time
#endif
    for(QnVideoCamera* camera: m_cameras.values())
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
    //SCOPED_MUTEX_LOCK( lock, &m_staticMtx);
    //static QnVideoCameraPool inst;
    //return &inst;
}

QnVideoCamera* QnVideoCameraPool::getVideoCamera(const QnResourcePtr& res)
{
    if (!dynamic_cast<const QnSecurityCamResource*>(res.data()))
        return 0;

    SCOPED_MUTEX_LOCK( lock, &m_staticMtx);
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
    SCOPED_MUTEX_LOCK( lock, &m_staticMtx);
    CameraMap::iterator itr = m_cameras.find(res);
    if (itr == m_cameras.end())
        return;
    delete itr.value();
    m_cameras.erase(itr);
}
