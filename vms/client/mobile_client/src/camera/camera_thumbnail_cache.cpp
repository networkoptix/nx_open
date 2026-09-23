// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_thumbnail_cache.h"

#include <memory>

#include <QtCore/QScopedPointerDeleteLater>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/thumbnails/camera_async_image_request.h>
#include <nx/vms/client/core/thumbnails/proxy_image_result.h>
#include <nx/vms/client/mobile/system_context.h>

namespace {

using namespace nx::vms::client::core;

const int kMinRefreshIntervalMs = 30000;
const int kMaxImageHeight = 200;

QString getThumbnailId(const nx::Uuid& id, const qint64 time)
{
    return id.toSimpleString() + ':' + QString::number(time);
}

// This class ensures that a camera thumbnail request is not sent again
// if the same request is being processed.
class CameraRequestManager: public QObject
{
    using Key = QPair<QnVirtualCameraResourcePtr, int>;
    QHash<Key, std::shared_ptr<AsyncImageResult>> m_requests;

public:
    /**
     * Requests a camera thumbnail. If the same image is already being loaded, the pending request
     * is reused instead of sending a duplicate one. The returned result may be ready already - for
     * example when the request has failed to be sent - in which case there will be no `ready`
     * signal, so the caller must check AsyncImageResult::isReady() first.
     */
    std::unique_ptr<AsyncImageResult> request(
        const QnVirtualCameraResourcePtr& camera, int maximumSize)
    {
        if (!NX_ASSERT(camera && camera->resourcePool()))
            return {};

        const auto key = Key(camera, maximumSize);

        if (const auto pendingRequest = m_requests.value(key))
            return std::make_unique<ProxyImageResult>(pendingRequest);

        const auto request = std::shared_ptr<AsyncImageResult>(
            new CameraAsyncImageRequest(camera, maximumSize), QScopedPointerDeleteLater{});

        if (request->isReady())
            return std::make_unique<ProxyImageResult>(request);

        connect(request.get(),
            &AsyncImageResult::ready,
            this,
            [this, key, source = request.get()]()
            {
                const auto it = m_requests.find(key);

                if (it == m_requests.cend() || it->get() != source)
                    return;

                m_requests.remove(key);
            });

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

struct QnCameraThumbnailCache::ThumbnailData
{
    QString thumbnailId;

    /**
     * While the image is being loaded - the moment the request has been sent, otherwise the
     * moment the last image has been obtained.
     */
    qint64 time = 0;

    /** Not null while the image is being loaded. */
    std::shared_ptr<nx::vms::client::core::AsyncImageResult> request;
};

QnCameraThumbnailCache::QnCameraThumbnailCache(
    nx::vms::client::mobile::SystemContext* context,
    QObject* parent)
    :
    QnThumbnailCacheBase(parent),
    SystemContextAware(context)
{
    connect(context, &nx::vms::client::core::SystemContext::remoteIdChanged, this,
        [this](nx::Uuid serverId)
        {
            if (serverId.isNull())
                stop();
            else
                start();
        });
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

    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        ThumbnailData& thumbnailData = m_thumbnailByResourceId[id];

        if (thumbnailData.request)
            return; //< The image is already being loaded.

        if (thumbnailData.time > 0
            && thumbnailData.time + kMinRefreshIntervalMs > m_elapsedTimer.elapsed())
        {
            return;
        }

        thumbnailData.time = m_elapsedTimer.elapsed();
    }

    auto request = cameraRequestManager()->request(camera, kMaxImageHeight);
    if (!NX_ASSERT(request))
        return;

    // AsyncImageResult is not thread-safe, so it is only accessed in this thread.
    if (request->isReady())
    {
        handleImageLoaded(id, request->image());
        return;
    }

    connect(request.get(),
        &AsyncImageResult::ready,
        this,
        [this, id, source = request.get()]()
        {
            QImage image;
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                const auto it = m_thumbnailByResourceId.find(id);

                if (it == m_thumbnailByResourceId.cend() || it->request.get() != source)
                    return;

                image = it->request->image();
                it->request.reset();
            }

            handleImageLoaded(id, image);
        });

    NX_MUTEX_LOCKER lock(&m_mutex);

    m_thumbnailByResourceId[id].request =
        std::shared_ptr<AsyncImageResult>(request.release(), QScopedPointerDeleteLater{});
}

void QnCameraThumbnailCache::handleImageLoaded(const nx::Uuid& id, const QImage& image)
{
    QString thumbnailId;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        const auto it = m_thumbnailByResourceId.find(id);
        if (it == m_thumbnailByResourceId.end())
            return;

        const auto pixmap = QPixmap::fromImage(image);
        if (pixmap.isNull())
        {
            // The request has failed or the image cannot be used. The previously loaded thumbnail
            // is kept, and `time` is intentionally left at the moment the failed request has been
            // sent, so that the next attempt is made in kMinRefreshIntervalMs.
            return;
        }

        thumbnailId = getThumbnailId(id, it->time);
        m_pixmaps.remove(it->thumbnailId);
        m_pixmaps.insert(thumbnailId, pixmap);

        it->thumbnailId = thumbnailId;
        it->time = m_elapsedTimer.elapsed();
    }

    emit thumbnailUpdated(id, thumbnailId);
}
