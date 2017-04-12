#pragma once

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>

#include <camera/thumbnail_cache_base.h>

class QnActiveCameraThumbnailLoaderPrivate;
class QnActiveCameraThumbnailLoader:
    public QObject,
    public QnThumbnailCacheBase,
    public QnConnectionContextAware
{
    Q_OBJECT

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(qint64 position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QUrl thumbnailUrl READ thumbnailUrl NOTIFY thumbnailUrlChanged)

public:
    QnActiveCameraThumbnailLoader(QObject *parent = nullptr);
    ~QnActiveCameraThumbnailLoader();

    QString resourceId() const;
    void setResourceId(const QString &id);

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
