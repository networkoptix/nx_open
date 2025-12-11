// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_thumbnail_cache.h"

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/media/jpeg.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/thumbnails/camera_async_image_request.h>
#include <nx/vms/client/core/thumbnails/proxy_image_result.h>
#include <nx/vms/client/mobile/system_context.h>

namespace {

using namespace nx::vms::client::core;

const int refreshInterval = 30000;
const int maxImageHeight = 200;

QString getThumbnailId(const nx::Uuid& id, const qint64 time)
{
    return id.toSimpleString() + ':' + QString::number(time);
}

// This class ensures that a camera thumbnail request is not sent again
// if the same request is being processed.
class CameraRequestManager: public QObject
{
    using Key = QPair<QnVirtualCameraResourcePtr, int>;
    QHash<Key, std::shared_ptr<CameraAsyncImageRequest>> m_requests;

public:
    std::unique_ptr<AsyncImageResult> request(
        const QnVirtualCameraResourcePtr& camera,
        std::function<void(const QImage& image)> callback,
        int maximumSize)
    {
        if (!NX_ASSERT(camera && camera->resourcePool()))
            return {};

        const auto key = Key(camera, maximumSize);
        if (m_requests.contains(key))
            return std::make_unique<ProxyImageResult>(m_requests.value(key));

        const auto request = std::make_shared<CameraAsyncImageRequest>(camera, maximumSize);

        if (request->isReady())
            return std::make_unique<ProxyImageResult>(request);

        const std::function<void()> onReady = nx::utils::guarded(this,
            [this, key, callback]()
            {
                if (callback) {
                    callback(m_requests.value(key)->image());
                }
                m_requests.remove(key);
                NX_VERBOSE(this, "Camera requests count: %1", m_requests.size());
            });

        connect(request.get(), &AsyncImageResult::ready, this, onReady);

        connect(camera->resourcePool(), &QnResourcePool::resourcesRemoved,
            this, &CameraRequestManager::handleResourcesRemoved, Qt::UniqueConnection);

        m_requests[key] = request;
        NX_VERBOSE(this, "Camera requests count: %1", m_requests.size());

        return std::make_unique<ProxyImageResult>(request);
    }

private:
    void handleResourcesRemoved(const QnResourceList& resources)
    {
        QSet<QnVirtualCameraResourcePtr> removedCameras;
        for (const auto& resource: resources)
        {
            if (auto cameraResource = resource.dynamicCast<QnVirtualCameraResource>())
                removedCameras.insert(cameraResource);
        }

        if (removedCameras.empty())
            return;

        for (auto it = m_requests.begin(); it != m_requests.end(); )
        {
            if (removedCameras.contains(it.key().first))
                it = m_requests.erase(it);
            else
                ++it;
        }
    }
};

static CameraRequestManager* cameraRequestManager()
{
    static CameraRequestManager instance;
    return &instance;
}

} // namespace

QnCameraThumbnailCache::QnCameraThumbnailCache(
    nx::vms::client::mobile::SystemContext* context,
    QObject* parent)
    :
    QnThumbnailCacheBase(parent),
    SystemContextAware(context)
{
}

QnCameraThumbnailCache::~QnCameraThumbnailCache()
{
}

void QnCameraThumbnailCache::start()
{
    if (m_elapsedTimer.isValid())
        return;

    for (const QnResourcePtr& resource: resourcePool()->getResources())
        at_resourcePool_resourceAdded(resource);

    connect(resourcePool(), &QnResourcePool::resourceAdded,
        this, &QnCameraThumbnailCache::at_resourcePool_resourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved,
        this, &QnCameraThumbnailCache::at_resourcePool_resourceRemoved);

    m_elapsedTimer.start();
}

void QnCameraThumbnailCache::stop()
{
    if (!m_elapsedTimer.isValid())
        return;

    resourcePool()->disconnect(this);

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_thumbnailByResourceId.clear();
    m_pixmaps.clear();
    m_elapsedTimer.invalidate();
}

QPixmap QnCameraThumbnailCache::getThumbnail(const QString& thumbnailId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_pixmaps.value(thumbnailId);
}

QString QnCameraThumbnailCache::cacheId() const
{
    return QString::number((intptr_t) this);
}

QString QnCameraThumbnailCache::thumbnailId(const nx::Uuid& resourceId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    const auto it = m_thumbnailByResourceId.find(resourceId);
    if (it == m_thumbnailByResourceId.end())
        return QString();

    return it->thumbnailId;
}

void QnCameraThumbnailCache::refreshThumbnails(const QList<nx::Uuid>& resourceIds)
{
    for (const nx::Uuid& id: resourceIds)
        refreshThumbnail(id);
}

void QnCameraThumbnailCache::at_resourcePool_resourceAdded(const QnResourcePtr& resource)
{
    if (!resource->hasFlags(Qn::live_cam))
        return;

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_thumbnailByResourceId.insert(resource->getId(), ThumbnailData());
}

void QnCameraThumbnailCache::at_resourcePool_resourceRemoved(const QnResourcePtr& resource)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto it = m_thumbnailByResourceId.find(resource->getId());
    if (it == m_thumbnailByResourceId.end())
        return;

    m_pixmaps.remove(it->thumbnailId);
    m_thumbnailByResourceId.erase(it);

    lock.unlock();

    emit thumbnailUpdated(resource->getId(), QString());
}

void QnCameraThumbnailCache::refreshThumbnail(const nx::Uuid& id)
{
    QnVirtualCameraResourcePtr camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id);
    if (!camera)
        return;

    NX_ASSERT(camera->systemContext() == systemContext());

    auto api = connectedServerApi();
    if (!api)
        return;

    NX_MUTEX_LOCKER lock(&m_mutex);

    ThumbnailData& thumbnailData = m_thumbnailByResourceId[id];

    if (thumbnailData.loading)
        return;

    if (thumbnailData.time > 0 && thumbnailData.time + refreshInterval > m_elapsedTimer.elapsed())
        return;

    auto callback = [this, id](const QImage& image)
    {
        ThumbnailData& thumbnailData = m_thumbnailByResourceId[id];
        if (image.isNull())
        {
            thumbnailData.loading = false;
            return;
        }

        bool thumbnailLoaded = false;
        QString thumbnailId;

        auto pixmap = QPixmap::fromImage(image);

        if (!pixmap.isNull())
        {
            thumbnailId = getThumbnailId(id, thumbnailData.time);
            m_pixmaps.remove(thumbnailData.thumbnailId);
            m_pixmaps.insert(thumbnailId, pixmap);
            thumbnailLoaded = true;
        }
        thumbnailData.thumbnailId = thumbnailId;
        thumbnailData.time = m_elapsedTimer.elapsed();
        thumbnailData.loading = false;

        if (thumbnailLoaded)
            emit thumbnailUpdated(id, thumbnailId);
    };

    auto request = cameraRequestManager()->request(camera, callback, maxImageHeight);

    if (!request)
    {
        thumbnailData.loading = false;
        return;
    }

    thumbnailData.time = m_elapsedTimer.elapsed();
    thumbnailData.loading = true;
}
