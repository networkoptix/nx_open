#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <nx/utils/uuid.h>

class QnThumbnailCacheAccessorPrivate;

class QnThumbnailCacheAccessor : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QnUuid resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(QString thumbnailId READ thumbnailId NOTIFY thumbnailIdChanged)
    Q_PROPERTY(QUrl thumbnailUrl READ thumbnailUrl NOTIFY thumbnailIdChanged)

public:
    QnThumbnailCacheAccessor(QObject* parent = nullptr);
    ~QnThumbnailCacheAccessor();

    QnUuid resourceId() const;
    void setResourceId(const QnUuid& resourceId);

    QString thumbnailId() const;
    QUrl thumbnailUrl() const;

public slots:
    void refreshThumbnail();

signals:
    void resourceIdChanged();
    void thumbnailIdChanged();

private:
    Q_DECLARE_PRIVATE(QnThumbnailCacheAccessor)
    QScopedPointer<QnThumbnailCacheAccessorPrivate> d_ptr;
};
