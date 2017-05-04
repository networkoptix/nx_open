#ifndef QN_GENERIC_IMAGES_H
#define QN_GENERIC_IMAGES_H

#include "platform_images.h"

class QnGenericImages: public QnPlatformImages {
    Q_OBJECT
public:
    QnGenericImages(QObject *parent = NULL): QnPlatformImages(parent) {}

    virtual QCursor bitmapCursor(Qt::CursorShape shape) const override {
        return QCursor(shape); /* Not really supported. */
    }
};

#endif // QN_GENERIC_IMAGES_H
