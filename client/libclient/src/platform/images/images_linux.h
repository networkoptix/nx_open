#ifndef QN_LINUX_IMAGES_H
#define QN_LINUX_IMAGES_H

#include "platform_images.h"

class QnLinuxImages: public QnPlatformImages {
    Q_OBJECT
public:
    QnLinuxImages(QObject *parent = NULL): QnPlatformImages(parent) {}

    virtual QCursor bitmapCursor(Qt::CursorShape shape) const override;
};

#endif // QN_LINUX_IMAGES_H
