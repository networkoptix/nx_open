#ifndef QNCAMERATHUMBNAILPROVIDER_H
#define QNCAMERATHUMBNAILPROVIDER_H

#include <QtQuick/QQuickImageProvider>

class QnCameraThumbnailCache;

class QnCameraThumbnailProvider : public QQuickImageProvider {
public:
    QnCameraThumbnailProvider();
    virtual QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;

    void setThumbnailCache(QnCameraThumbnailCache *thumbnailCache);

private:
    QnCameraThumbnailCache *m_thumbnailCache;
};

#endif // QNCAMERATHUMBNAILPROVIDER_H
