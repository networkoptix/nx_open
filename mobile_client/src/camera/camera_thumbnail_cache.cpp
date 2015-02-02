#include "camera_thumbnail_cache.h"

#include "core/resource_management/resource_pool.h"
#include "core/resource/security_cam_resource.h"
#include "core/resource/media_server_resource.h"

#include <QtCore/QTimer>

namespace {

    const int refreshInterval = 30000;
    const QSize defaultSize(0, 200);

} // anonymous namespace

QnCameraThumbnailCache::QnCameraThumbnailCache(QObject *parent) :
    QObject(parent),
    m_thumbnailSize(defaultSize)
{
}

QnCameraThumbnailCache::~QnCameraThumbnailCache() {
}

void QnCameraThumbnailCache::start() {
    for (const QnResourcePtr &resource: qnResPool->getResources())
        at_resourcePool_resourceAdded(resource);

    connect(qnResPool,  &QnResourcePool::resourceAdded,     this,   &QnCameraThumbnailCache::at_resourcePool_resourceAdded);
    connect(qnResPool,  &QnResourcePool::resourceRemoved,   this,   &QnCameraThumbnailCache::at_resourcePool_resourceRemoved);

    m_elapsedTimer.start();
}

void QnCameraThumbnailCache::stop() {
    disconnect(qnResPool, 0, this, 0);

    m_thumbnailByResourceId.clear();
    m_pixmaps.clear();
}

QPixmap QnCameraThumbnailCache::thumbnail(const QString &thumbnailId) const {
    QMutexLocker lock(&m_mutex);
    return m_pixmaps.value(thumbnailId);
}

QString QnCameraThumbnailCache::thumbnailId(const QnUuid &resourceId) {
    refreshThumbnail(resourceId);

    QMutexLocker lock(&m_mutex);

    auto it = m_thumbnailByResourceId.find(resourceId);
    if (it == m_thumbnailByResourceId.end())
        return QString();

    return it->thumbnailId;
}

void QnCameraThumbnailCache::refreshThumbnails(const QList<QnUuid> &resourceIds) {
    for (const QnUuid &id: resourceIds) {
        if (!m_thumbnailByResourceId.contains(id))
            continue;
        refreshThumbnail(id);
    }
}

void QnCameraThumbnailCache::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    if (!resource->hasFlags(Qn::live_cam))
        return;

    m_thumbnailByResourceId.insert(resource->getId(), ThumbnailData());
}

void QnCameraThumbnailCache::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QMutexLocker lock(&m_mutex);

    auto it = m_thumbnailByResourceId.find(resource->getId());
    if (it == m_thumbnailByResourceId.end())
        return;

    m_pixmaps.remove(it->thumbnailId);
    m_thumbnailByResourceId.erase(it);
}

void QnCameraThumbnailCache::at_thumbnailReceived(int status, const QImage &thumbnail, int handle) {
    QMutexLocker lock(&m_mutex);

    QnUuid id = m_idByRequestHandle.take(handle);
    if (id.isNull())
        return;

    ThumbnailData &thumbnailData = m_thumbnailByResourceId[id];

    QString thumbnailId;
    if (status == 0) {
        QString thumbnailId = id.toString();
        if (thumbnailId.startsWith(QLatin1Char('{')))
            thumbnailId = thumbnailId.mid(1, thumbnailId.size() - 2);
        thumbnailId += lit(":");
        thumbnailId += QString::number(thumbnailData.time);
        m_pixmaps.remove(thumbnailData.thumbnailId);
        m_pixmaps.insert(thumbnailId, QPixmap::fromImage(thumbnail));
        thumbnailData.thumbnailId = thumbnailId;
    }

    thumbnailData.time = m_elapsedTimer.elapsed();
    thumbnailData.loading = false;

    if (status == 0) {
        lock.unlock();
        emit thumbnailUpdated(id, thumbnailId);
    }
}

void QnCameraThumbnailCache::refreshThumbnail(const QnUuid &id) {
    QMutexLocker lock(&m_mutex);

    ThumbnailData &thumbnailData = m_thumbnailByResourceId[id];

    if (thumbnailData.loading)
        return;

    if (thumbnailData.time > 0 && thumbnailData.time + refreshInterval > m_elapsedTimer.elapsed())
        return;

    QnNetworkResourcePtr camera = qnResPool->getResourceById(id).dynamicCast<QnNetworkResource>();
    if (!camera || camera->getStatus() != Qn::Online)
        return;

    QnMediaServerResourcePtr server = camera->getParentResource().dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    int handle = server->apiConnection()->getThumbnailAsync(
                camera, -1, -1, m_thumbnailSize,
                lit("jpg"), QnMediaServerConnection::IFrameAfterTime,
                this, SLOT(at_thumbnailReceived(int, const QImage&, int)));

    if (handle == -1) {
        thumbnailData.loading = false;
        return;
    }

    thumbnailData.time = m_elapsedTimer.elapsed();
    thumbnailData.loading = true;
    m_idByRequestHandle[handle] = id;
}
