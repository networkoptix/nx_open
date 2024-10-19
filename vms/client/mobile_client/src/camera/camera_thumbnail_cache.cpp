// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_thumbnail_cache.h"

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/media/jpeg.h>
#include <nx/utils/guarded_callback.h>

namespace {

    const int refreshInterval = 30000;
    const QSize defaultSize(0, 200);

    QString getThumbnailId(const nx::Uuid &id, const qint64 time)
    {
        QString result = id.toString();
        if (result.startsWith(QLatin1Char('{')))
            result = result.mid(1, result.size() - 2);
        result += ':';
        result += QString::number(time);
        return result;
    }

} // anonymous namespace

template<> QnCameraThumbnailCache* Singleton<QnCameraThumbnailCache>::s_instance = nullptr;

QnCameraThumbnailCache::QnCameraThumbnailCache(QObject *parent)
    : QObject(parent)
    , m_decompressThread(new QThread(this))
{
    m_request.size = defaultSize;

    // Getting screenshot from NVR can be very long, so mobile client hangs trying to request some.
    m_request.ignoreExternalArchive = true;
    m_decompressThread->setObjectName("QnCameraThumbnailCache_decompressThread");
    m_decompressThread->start();
}

QnCameraThumbnailCache::~QnCameraThumbnailCache()
{
    m_decompressThread->quit();
    m_decompressThread->wait();
}

void QnCameraThumbnailCache::start()
{
    if (m_elapsedTimer.isValid())
        return;

    for (const QnResourcePtr &resource: resourcePool()->getResources())
        at_resourcePool_resourceAdded(resource);

    connect(resourcePool(),  &QnResourcePool::resourceAdded,     this,   &QnCameraThumbnailCache::at_resourcePool_resourceAdded);
    connect(resourcePool(),  &QnResourcePool::resourceRemoved,   this,   &QnCameraThumbnailCache::at_resourcePool_resourceRemoved);

    m_elapsedTimer.start();
}

void QnCameraThumbnailCache::stop()
{
    if (!m_elapsedTimer.isValid())
        return;

    resourcePool()->disconnect(this);

    m_thumbnailByResourceId.clear();
    m_pixmaps.clear();
    m_elapsedTimer.invalidate();
}

QPixmap QnCameraThumbnailCache::getThumbnail(const QString &thumbnailId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_pixmaps.value(thumbnailId);
}

QString QnCameraThumbnailCache::thumbnailId(const nx::Uuid &resourceId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    const auto it = m_thumbnailByResourceId.find(resourceId);
    if (it == m_thumbnailByResourceId.end())
        return QString();

    return it->thumbnailId;
}

void QnCameraThumbnailCache::refreshThumbnails(const QList<nx::Uuid> &resourceIds)
{
    for (const nx::Uuid &id: resourceIds)
        refreshThumbnail(id);
}

void QnCameraThumbnailCache::at_resourcePool_resourceAdded(const QnResourcePtr &resource)
{
    if (!resource->hasFlags(Qn::live_cam))
        return;

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_thumbnailByResourceId.insert(resource->getId(), ThumbnailData());
}

void QnCameraThumbnailCache::at_resourcePool_resourceRemoved(const QnResourcePtr &resource)
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

void QnCameraThumbnailCache::refreshThumbnail(const nx::Uuid &id)
{
    QnVirtualCameraResourcePtr camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id);
    if (!camera)
        return;

    auto api = connectedServerApi();
    if (!api)
        return;

    NX_MUTEX_LOCKER lock(&m_mutex);

    ThumbnailData &thumbnailData = m_thumbnailByResourceId[id];

    if (thumbnailData.loading)
        return;

    if (thumbnailData.time > 0 && thumbnailData.time + refreshInterval > m_elapsedTimer.elapsed())
        return;

    const auto handleReply =
        [this, id](
            bool success,
            rest::Handle /*requestId*/,
            QByteArray imageData,
            const nx::network::http::HttpHeaders& /*headers*/)
        {
            bool thumbnailLoaded = false;
            QString thumbnailId;

            {
                QPixmap pixmap;
                if (success && !imageData.isEmpty())
                    pixmap = QPixmap::fromImage(decompressJpegImage(imageData));

                NX_MUTEX_LOCKER lock(&m_mutex);

                ThumbnailData &thumbnailData = m_thumbnailByResourceId[id];

                if (success)
                {
                    if (!pixmap.isNull())
                    {
                        thumbnailId = getThumbnailId(id, thumbnailData.time);
                        m_pixmaps.remove(thumbnailData.thumbnailId);
                        m_pixmaps.insert(thumbnailId, pixmap);
                        thumbnailLoaded = true;
                    }
                    thumbnailData.thumbnailId = thumbnailId;
                    thumbnailData.time = m_elapsedTimer.elapsed();
                }

                thumbnailData.loading = false;
            }

            if (thumbnailLoaded)
                emit thumbnailUpdated(id, thumbnailId);
        };

    m_request.camera = camera;
    int handle = api->cameraThumbnailAsync(
        m_request, nx::utils::guarded(this, handleReply), m_decompressThread);

    if (handle == -1)
    {
        thumbnailData.loading = false;
        return;
    }

    thumbnailData.time = m_elapsedTimer.elapsed();
    thumbnailData.loading = true;
}
