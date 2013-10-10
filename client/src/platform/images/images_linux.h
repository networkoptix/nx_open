#ifndef QN_UNIX_IMAGES_H
#define QN_UNIX_IMAGES_H

#include "platform_images.h"

class QnUnixImages: public QnPlatformImages {
    Q_OBJECT
public:
    QnUnixImages(QObject *parent = NULL): QnPlatformImages(parent) {}

    virtual QCursor bitmapCursor(Qt::CursorShape shape) const override;
};

#endif // QN_UNIX_IMAGES_H
