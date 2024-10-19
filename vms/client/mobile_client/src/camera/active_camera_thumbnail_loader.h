// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <camera/thumbnail_cache_base.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/mobile/current_system_context_aware.h>

class QnActiveCameraThumbnailLoaderPrivate;
class QnActiveCameraThumbnailLoader:
    public QObject,
    public QnThumbnailCacheBase,
    public nx::vms::client::mobile::CurrentSystemContextAware
{
    Q_OBJECT

    Q_PROPERTY(nx::Uuid resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(qint64 position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QUrl thumbnailUrl READ thumbnailUrl NOTIFY thumbnailUrlChanged)

public:
    QnActiveCameraThumbnailLoader(QObject *parent = nullptr);
    ~QnActiveCameraThumbnailLoader();

    nx::Uuid resourceId() const;
    void setResourceId(const nx::Uuid &id);

    qint64 position() const;
    void setPosition(qint64 position);

    virtual QPixmap getThumbnail(const QString &thumbnailId) const override;

    QString thumbnailId() const;
    QUrl thumbnailUrl() const;

    Q_INVOKABLE void forceLoadThumbnail(qint64 position);
    Q_INVOKABLE void initialize(QObject *item);

signals:
    void resourceIdChanged();
    void positionChanged();
    void thumbnailIdChanged();
    void thumbnailUrlChanged();

private:
    Q_DECLARE_PRIVATE(QnActiveCameraThumbnailLoader)

    QScopedPointer<QnActiveCameraThumbnailLoaderPrivate> d_ptr;
};
