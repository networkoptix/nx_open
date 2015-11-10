#pragma once
#include <QtQuick/QQuickImageProvider>

class QnThumbnailCacheBase;
class QnCameraThumbnailProvider : public QQuickImageProvider {
public:
    QnCameraThumbnailProvider();
    virtual QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;

    void setThumbnailCache(QnThumbnailCacheBase *thumbnailCache);

private:
    QnThumbnailCacheBase *m_thumbnailCache;
};
