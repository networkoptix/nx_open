#include "active_camera_thumbnail_loader.h"
#include "private/active_camera_thumbnail_loader_p.h"

#include <QtQml/QtQml>

#include <utils/common/id.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <ui/camera_thumbnail_provider.h>

QnActiveCameraThumbnailLoader::QnActiveCameraThumbnailLoader(QObject *parent)
    : QObject(parent)
    , d_ptr(new QnActiveCameraThumbnailLoaderPrivate(this))
{
    connect(this, &QnActiveCameraThumbnailLoader::thumbnailIdChanged, this, &QnActiveCameraThumbnailLoader::thumbnailUrlChanged);
}

QnActiveCameraThumbnailLoader::~QnActiveCameraThumbnailLoader() {
    Q_D(QnActiveCameraThumbnailLoader);

    if (d->imageProvider)
        d->imageProvider->setThumbnailCache(nullptr);
}

QString QnActiveCameraThumbnailLoader::resourceId() const {
    Q_D(const QnActiveCameraThumbnailLoader);
    if (!d->camera)
        return QString();

    return d->camera->getId().toString();
}

qint64 QnActiveCameraThumbnailLoader::position() const {
    Q_D(const QnActiveCameraThumbnailLoader);
    return d->position;
}

void QnActiveCameraThumbnailLoader::setPosition(qint64 position) {
    Q_D(QnActiveCameraThumbnailLoader);

    if (d->position == position)
        return;

    d->position = position;
    d->requestRefresh();
    emit positionChanged();
}

void QnActiveCameraThumbnailLoader::setResourceId(const QString &id) {
    Q_D(QnActiveCameraThumbnailLoader);

    if (resourceId() == id)
        return;

    d->clear();
    d->camera = resourcePool()->getResourceById<QnVirtualCameraResource>(QnUuid::fromStringSafe(id));
    d->refresh();

    emit resourceIdChanged();
    emit thumbnailIdChanged();
}

QPixmap QnActiveCameraThumbnailLoader::getThumbnail(const QString &thumbnailId) const {
    Q_UNUSED(thumbnailId)

    Q_D(const QnActiveCameraThumbnailLoader);
    return d->thumbnail();
}

QString QnActiveCameraThumbnailLoader::thumbnailId() const {
    Q_D(const QnActiveCameraThumbnailLoader);
    return d->thumbnailId;
}

QUrl QnActiveCameraThumbnailLoader::thumbnailUrl() const {
    QString id = thumbnailId();
    if (id.isNull())
        return QUrl();
    return QUrl(lit("image://%1/%2").arg(lit("active")).arg(id));
}

void QnActiveCameraThumbnailLoader::forceLoadThumbnail(qint64 position) {
    Q_D(QnActiveCameraThumbnailLoader);
    bool isPositionChanged = d->position != position;

    d->position = position;
    d->refresh(true);

    if (isPositionChanged)
        emit positionChanged();
}

void QnActiveCameraThumbnailLoader::initialize(QObject *item) {
    Q_D(QnActiveCameraThumbnailLoader);

    QQmlEngine *engine = qmlEngine(item);
    if (!engine)
        return;

    d->imageProvider = dynamic_cast<QnCameraThumbnailProvider*>(engine->imageProvider(lit("active")));
    if (!d->imageProvider)
        return;

    d->imageProvider->setThumbnailCache(this);
}
