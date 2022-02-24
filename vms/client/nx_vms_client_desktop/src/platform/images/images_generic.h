// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_GENERIC_IMAGES_H
#define QN_GENERIC_IMAGES_H

#include "platform_images.h"

class QnGenericImages: public QnPlatformImages {
    Q_OBJECT
public:
    QnGenericImages(QObject *parent = nullptr): QnPlatformImages(parent) {}

    virtual QCursor bitmapCursor(Qt::CursorShape shape) const override {
        return QCursor(shape); /* Not really supported. */
    }
};

#endif // QN_GENERIC_IMAGES_H
