#include "camera_pool.h"

#include <nx/utils/std/cpp14.h>

#include <core/resource/resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>

#ifdef Q_OS_WIN
#   include "plugins/storage/dts/vmax480/vmax480_stream_fetcher.h"
#endif

#include "video_camera.h"

//-------------------------------------------------------------------------------------------------
// VideoCameraLocker
VideoCameraLocker::VideoCameraLocker(QnVideoCameraPtr camera):
    m_camera(camera)
{
    m_camera->inUse(this);
}

VideoCameraLocker::~VideoCameraLocker()
{
    m_camera->notInUse(this);
}

//-------------------------------------------------------------------------------------------------
// QnVideoCameraPool


void QnVideoCameraPool::stop()
{
    for( const QnVideoCameraPtr& camera: m_cameras.values())
        camera->beforeStop();

#if defined(Q_OS_WIN) && defined(ENABLE_VMAX)
        VMaxStreamFetcher::pleaseStopAll(); // increase stop time
#endif

    m_cameras.clear();
}

QnVideoCameraPool::QnVideoCameraPool(
    const MSSettings& settings,
    QnResourcePool* resourcePool)
    :
    m_settings(settings),
    m_resourcePool(resourcePool)
{
}

QnVideoCameraPool::~QnVideoCameraPool()
{
    stop();
}

void QnVideoCameraPool::updateActivity()
{
    QnMutexLocker lock(&m_mutex);
    for (const auto&camera: m_cameras)
        camera->updateActivity();
}

QnVideoCameraPtr QnVideoCameraPool::getVideoCamera(const QnResourcePtr& res) const
{
    if (!dynamic_cast<const QnSecurityCamResource*>(res.data()))
        return QnVideoCameraPtr();
    QnMutexLocker lock(&m_mutex);
    CameraMap::const_iterator itr = m_cameras.find(res);
    return itr == m_cameras.cend() ? QnVideoCameraPtr() : itr.value();
}

QnVideoCameraPtr QnVideoCameraPool::addVideoCamera(const QnResourcePtr& res)
{
    if (!dynamic_cast<const QnSecurityCamResource*>(res.data()))
        return QnVideoCameraPtr();
    QnMutexLocker lock(&m_mutex);
    return m_cameras.insert(res, QnVideoCameraPtr(new QnVideoCamera(m_settings, res))).value();
}

bool QnVideoCameraPool::addVideoCamera(const QnResourcePtr& res, QnVideoCameraPtr camera)
{
    if (!dynamic_cast<const QnSecurityCamResource*>(res.data()))
        return false;
    QnMutexLocker lock(&m_mutex);
    m_cameras.insert(res, camera);
    return true;
}

void QnVideoCameraPool::removeVideoCamera(const QnResourcePtr& res)
{
    QnMutexLocker lock( &m_mutex );
    m_cameras.remove( res );
}

std::unique_ptr<VideoCameraLocker>
    QnVideoCameraPool::getVideoCameraLockerByResourceId(const QnUuid& id) const
{
    QnResourcePtr resource = m_resourcePool->getResourceById(id);
    if (!resource)
        return nullptr;

    QnSecurityCamResourcePtr cameraResource = resource.dynamicCast<QnSecurityCamResource>();
    if (!cameraResource)
        return nullptr;

    auto camera = getVideoCamera(resource);
    if (!camera)
        return nullptr;

    return std::make_unique<VideoCameraLocker>(camera);
}
