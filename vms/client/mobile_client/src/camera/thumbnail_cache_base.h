// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QPixmap>

class QnThumbnailCacheBase {
public:
    virtual QPixmap getThumbnail(const QString &thumbnailId) const = 0;
};
