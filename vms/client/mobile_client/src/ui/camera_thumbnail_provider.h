// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once
#include <QtQuick/QQuickImageProvider>

// TODO: Get id of this class and use the one from the core module. #ynikitenkov #ikulaychuk
class QnThumbnailCacheBase;
class QnCameraThumbnailProvider : public QQuickImageProvider {
public:
    QnCameraThumbnailProvider();
    virtual QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;

    void setThumbnailCache(QnThumbnailCacheBase *thumbnailCache);

private:
    QnThumbnailCacheBase *m_thumbnailCache;
};
