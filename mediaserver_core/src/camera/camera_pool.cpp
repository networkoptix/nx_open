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
    for( const QnVideoCameraPtr& camera: m_cameras.values())
        camera->beforeStop();

#if defined(Q_OS_WIN) && defined(ENABLE_VMAX)
        VMaxStreamFetcher::pleaseStopAll(); // increase stop time
#endif

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
    //QnMutexLocker lock( &m_staticMtx );
    //static QnVideoCameraPool inst;
    //return &inst;
}

void QnVideoCameraPool::updateActivity()
{
    QnMutexLocker lock(&m_staticMtx);
    for (const auto&camera: m_cameras)
        camera->updateActivity();
}

QnVideoCameraPtr QnVideoCameraPool::getVideoCamera(const QnResourcePtr& res) const
{
    if (!dynamic_cast<const QnSecurityCamResource*>(res.data()))
        return QnVideoCameraPtr();
    QnMutexLocker lock(&m_staticMtx);
    CameraMap::const_iterator itr = m_cameras.find(res);
    return itr == m_cameras.cend() ? QnVideoCameraPtr() : itr.value();
}

QnVideoCameraPtr QnVideoCameraPool::addVideoCamera(const QnResourcePtr& res)
{
    if (!dynamic_cast<const QnSecurityCamResource*>(res.data()))
        return QnVideoCameraPtr();
    QnMutexLocker lock(&m_staticMtx);
    return m_cameras.insert(res, QnVideoCameraPtr(new QnVideoCamera(res))).value();
}

void QnVideoCameraPool::removeVideoCamera(const QnResourcePtr& res)
{
    QnMutexLocker lock( &m_staticMtx );
    m_cameras.remove( res );
}
