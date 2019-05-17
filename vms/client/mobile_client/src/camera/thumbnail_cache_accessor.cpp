#include "thumbnail_cache_accessor.h"

#include "camera_thumbnail_cache.h"

class QnThumbnailCacheAccessorPrivate
{
    Q_DECLARE_PUBLIC(QnThumbnailCacheAccessor)
    QnThumbnailCacheAccessor* q_ptr;

public:
    QnUuid resourceId;
    QString thumbnailId;

    QnThumbnailCacheAccessorPrivate(QnThumbnailCacheAccessor* parent);

    void setThumbnailId(const QString& id);
};

QnThumbnailCacheAccessor::QnThumbnailCacheAccessor(QObject* parent) :
    QObject(parent),
    d_ptr(new QnThumbnailCacheAccessorPrivate(this))
{
    Q_D(QnThumbnailCacheAccessor);

    const auto thumbnailCache = QnCameraThumbnailCache::instance();
    connect(thumbnailCache, &QnCameraThumbnailCache::thumbnailUpdated,
        this, [d](const QnUuid &resourceId, const QString &thumbnailId)
        {
            if (resourceId == d->resourceId)
                d->setThumbnailId(thumbnailId);
        }
    );
}

QnThumbnailCacheAccessor::~QnThumbnailCacheAccessor()
{
}

QString QnThumbnailCacheAccessor::resourceId() const
{
    Q_D(const QnThumbnailCacheAccessor);
    return d->resourceId.toString();
}

void QnThumbnailCacheAccessor::setResourceId(const QString& resourceId)
{
    Q_D(QnThumbnailCacheAccessor);
    if (d->resourceId.toString() == resourceId)
        return;

    d->resourceId = QnUuid::fromStringSafe(resourceId);
    emit resourceIdChanged();

    if (d->resourceId.isNull())
        d->setThumbnailId(QString());
    else
        d->setThumbnailId(QnCameraThumbnailCache::instance()->thumbnailId(d->resourceId));
}

QString QnThumbnailCacheAccessor::thumbnailId() const
{
    Q_D(const QnThumbnailCacheAccessor);
    return d->thumbnailId;
}

QUrl QnThumbnailCacheAccessor::thumbnailUrl() const
{
    Q_D(const QnThumbnailCacheAccessor);
    if (d->thumbnailId.isEmpty())
        return QUrl();

    return QUrl("image://thumbnail/" + d->thumbnailId);
}

void QnThumbnailCacheAccessor::refreshThumbnail()
{
    Q_D(const QnThumbnailCacheAccessor);
    if (d->resourceId.isNull())
        return;

    QnCameraThumbnailCache::instance()->refreshThumbnail(d->resourceId);
}

QnThumbnailCacheAccessorPrivate::QnThumbnailCacheAccessorPrivate(QnThumbnailCacheAccessor* parent):
    q_ptr(parent)
{
}

void QnThumbnailCacheAccessorPrivate::setThumbnailId(const QString& id)
{
    if (thumbnailId == id)
        return;

    thumbnailId = id;

    Q_Q(QnThumbnailCacheAccessor);
    emit q->thumbnailIdChanged();
}
