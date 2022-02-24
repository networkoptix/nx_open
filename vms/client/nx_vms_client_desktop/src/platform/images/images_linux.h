// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_LINUX_IMAGES_H
#define QN_LINUX_IMAGES_H

#include "platform_images.h"

class QnLinuxImages: public QnPlatformImages {
    Q_OBJECT
public:
    QnLinuxImages(QObject *parent = nullptr): QnPlatformImages(parent) {}

    virtual QCursor bitmapCursor(Qt::CursorShape shape) const override;
};

#endif // QN_LINUX_IMAGES_H
