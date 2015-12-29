#include "camera_thumbnail_cache.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>
#include <common/common_module.h>

namespace {

    const int refreshInterval = 30000;
    const QSize defaultSize(0, 200);

    QString getThumbnailId(const QnUuid &id, const qint64 time)
    {
        QString result = id.toString();
        if (result.startsWith(QLatin1Char('{')))
            result = result.mid(1, result.size() - 2);
        result += lit(":");
        result += QString::number(time);
        return result;
    }

} // anonymous namespace

QnCameraThumbnailCache::QnCameraThumbnailCache(QObject *parent)
    : QObject(parent)
{
    m_request.roundMethod = QnThumbnailRequestData::KeyFrameAfterMethod;
}

QnCameraThumbnailCache::~QnCameraThumbnailCache()
{
}

void QnCameraThumbnailCache::start()
{
    for (const QnResourcePtr &resource: qnResPool->getResources())
        at_resourcePool_resourceAdded(resource);

    connect(qnResPool,  &QnResourcePool::resourceAdded,     this,   &QnCameraThumbnailCache::at_resourcePool_resourceAdded);
    connect(qnResPool,  &QnResourcePool::resourceRemoved,   this,   &QnCameraThumbnailCache::at_resourcePool_resourceRemoved);

    m_elapsedTimer.start();
}

void QnCameraThumbnailCache::stop()
{
    disconnect(qnResPool, 0, this, 0);

    m_thumbnailByResourceId.clear();
    m_pixmaps.clear();
}

QPixmap QnCameraThumbnailCache::getThumbnail(const QString &thumbnailId) const
{
    QnMutexLocker lock(&m_mutex);
    return m_pixmaps.value(thumbnailId);
}

QString QnCameraThumbnailCache::thumbnailId(const QnUuid &resourceId)
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_thumbnailByResourceId.find(resourceId);
    if (it == m_thumbnailByResourceId.end())
        return QString();

    QString thumbnailId = it->thumbnailId;

    lock.unlock();

    refreshThumbnail(resourceId);

    return thumbnailId;
}

void QnCameraThumbnailCache::refreshThumbnails(const QList<QnUuid> &resourceIds)
{
    QSet<QnUuid> ids;
    {
        QnMutexLocker lock(&m_mutex);
        ids = QSet<QnUuid>::fromList(m_thumbnailByResourceId.keys());
    }

    for (const QnUuid &id: resourceIds)
    {
        if (!ids.contains(id))
            continue;
        refreshThumbnail(id);
    }
}

void QnCameraThumbnailCache::at_resourcePool_resourceAdded(const QnResourcePtr &resource)
{
    if (!resource->hasFlags(Qn::live_cam))
        return;

    QnMutexLocker lock(&m_mutex);
    m_thumbnailByResourceId.insert(resource->getId(), ThumbnailData());
}

void QnCameraThumbnailCache::at_resourcePool_resourceRemoved(const QnResourcePtr &resource)
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_thumbnailByResourceId.find(resource->getId());
    if (it == m_thumbnailByResourceId.end())
        return;

    m_pixmaps.remove(it->thumbnailId);
    m_thumbnailByResourceId.erase(it);

    lock.unlock();

    emit thumbnailUpdated(resource->getId(), QString());
}

void QnCameraThumbnailCache::refreshThumbnail(const QnUuid &id)
{
    QnMutexLocker lock(&m_mutex);

    ThumbnailData &thumbnailData = m_thumbnailByResourceId[id];

    if (thumbnailData.loading)
        return;

    if (thumbnailData.time > 0 && thumbnailData.time + refreshInterval > m_elapsedTimer.elapsed())
        return;

    QnVirtualCameraResourcePtr camera = qnResPool->getResourceById<QnVirtualCameraResource>(id);
    if (!camera)
        return;

    QnMediaServerResourcePtr server = qnCommon->currentServer();
    if (!server)
        return;

    m_request.camera = camera;

    auto handleReply = [this, id] (bool success, rest::Handle handleId, const QByteArray &imageData)
    {
        Q_UNUSED(handleId)

        QString thumbnailId;
        {
            QnMutexLocker lock(&m_mutex);

            ThumbnailData &thumbnailData = m_thumbnailByResourceId[id];

            if (success)
            {
                thumbnailId = getThumbnailId(id.toString(), thumbnailData.time);
                m_pixmaps.remove(thumbnailData.thumbnailId);
                m_pixmaps.insert(thumbnailId, QPixmap::fromImage(QImage::fromData(imageData, "JPG")));
                thumbnailData.thumbnailId = thumbnailId;
            }

            thumbnailData.time = m_elapsedTimer.elapsed();
            thumbnailData.loading = false;
        }

        if (success)
            emit thumbnailUpdated(id, thumbnailId);
    };

    int handle = server->restConnection()->cameraThumbnailAsync(m_request, handleReply, QThread::currentThread());

    if (handle == -1) {
        thumbnailData.loading = false;
        return;
    }

    thumbnailData.time = m_elapsedTimer.elapsed();
    thumbnailData.loading = true;
}
