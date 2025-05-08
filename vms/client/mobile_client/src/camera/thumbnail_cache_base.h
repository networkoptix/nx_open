// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtGui/QPixmap>

class QnThumbnailCacheBase: public QObject
{
    Q_OBJECT

public:
    explicit QnThumbnailCacheBase(QObject* parent): QObject(parent) {}

    virtual QString cacheId() const = 0;
    virtual QPixmap getThumbnail(const QString &thumbnailId) const = 0;
};
