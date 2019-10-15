#include "camera_pool.h"

#include <nx/utils/std/cpp14.h>

#include <core/resource/resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>

#if defined(Q_OS_WIN)
    #include "plugins/storage/dts/vmax480/vmax480_stream_fetcher.h"
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
    {
        QnMutexLocker lock(&m_mutex);
        m_isStopped = true; //< Make sure m_cameras will not be modifyed.
    }

    for( const QnVideoCameraPtr& camera: m_cameras.values())
        camera->stopAndCleanup();

    #if defined(Q_OS_WIN) && defined(ENABLE_VMAX)
        VMaxStreamFetcher::pleaseStopAll(); //< increase stop time
    #endif

    m_cameras.clear();
}

QnVideoCameraPool::QnVideoCameraPool(
    const nx::vms::server::Settings& settings,
    QnDataProviderFactory* dataProviderFactory,
    QnResourcePool* resourcePool)
    :
    m_settings(settings),
    m_dataProviderFactory(dataProviderFactory),
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
    if (m_isStopped)
        return;

    for (const auto& camera: m_cameras)
        camera->updateActivity();
}

QnVideoCameraPtr QnVideoCameraPool::getVideoCamera(const QnResourcePtr& res) const
{
    if (!dynamic_cast<const QnSecurityCamResource*>(res.data()))
        return {};

    QnMutexLocker lock(&m_mutex);
    if (m_isStopped)
        return {};

    CameraMap::const_iterator itr = m_cameras.find(res);
    return itr == m_cameras.cend() ? QnVideoCameraPtr() : itr.value();
}

QnVideoCameraPtr QnVideoCameraPool::addVideoCamera(const QnResourcePtr& res)
{
    if (!dynamic_cast<const QnSecurityCamResource*>(res.data()))
        return {};

    QnMutexLocker lock(&m_mutex);
    if (m_isStopped)
        return {};

    return m_cameras.insert(
        res, QnVideoCameraPtr(new QnVideoCamera(m_settings, m_dataProviderFactory, res))).value();
}

bool QnVideoCameraPool::addVideoCamera(const QnResourcePtr& res, QnVideoCameraPtr camera)
{
    if (!dynamic_cast<const QnSecurityCamResource*>(res.data()))
        return false;

    QnMutexLocker lock(&m_mutex);
    if (m_isStopped)
        return true;

    m_cameras.insert(res, camera);
    return true;
}

void QnVideoCameraPool::removeVideoCamera(const QnResourcePtr& res)
{
    QnVideoCameraPtr removedCamera;
    {
        QnMutexLocker lock( &m_mutex );
        if (m_isStopped)
            return;
        auto itr = m_cameras.find(res);
        if (itr == m_cameras.end())
            return;
        removedCamera = itr.value();
        m_cameras.erase(itr);
    }
    // Ensure readers are stopped and can not be started again before ~QnVideoCamera().
    removedCamera->stopAndCleanup();
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
