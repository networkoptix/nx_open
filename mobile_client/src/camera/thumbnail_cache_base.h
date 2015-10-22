#pragma once

#include <QtGui/QPixmap>

class QnThumbnailCacheBase {
public:
    virtual QPixmap getThumbnail(const QString &thumbnailId) const = 0;
};
