#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>

class QnThumbnailCacheAccessorPrivate;

class QnThumbnailCacheAccessor : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(QString thumbnailId READ thumbnailId NOTIFY thumbnailIdChanged)
    Q_PROPERTY(QUrl thumbnailUrl READ thumbnailUrl NOTIFY thumbnailIdChanged)

public:
    QnThumbnailCacheAccessor(QObject* parent = nullptr);
    ~QnThumbnailCacheAccessor();

    QString resourceId() const;
    void setResourceId(const QString& resourceId);

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
