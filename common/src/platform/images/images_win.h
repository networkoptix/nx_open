#ifndef QN_WINDOWS_IMAGES_H
#define QN_WINDOWS_IMAGES_H

#include "platform_images.h"

class QnWindowsImages: public QnPlatformImages {
    Q_OBJECT;
    typedef QnPlatformImages base_type;

public:
    QnWindowsImages(QObject *parent = NULL);
    virtual ~QnWindowsImages();

    virtual QCursor bitmapCursor(Qt::CursorShape shape) const override;
};

#endif // QN_WINDOWS_IMAGES_H
