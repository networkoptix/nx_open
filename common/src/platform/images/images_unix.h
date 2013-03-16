#ifndef QN_X11_IMAGES_H
#define QN_X11_IMAGES_H

#include <QtGui/QPixmap>
#include "platform_images.h"

class QnX11Images: public QnPlatformImages {
    Q_OBJECT

    typedef QnPlatformImages base_type;

public:
    QnX11Images(QObject *parent = NULL);
    virtual ~QnX11Images();

    virtual QCursor bitmapCursor(Qt::CursorShape shape) const override;
};

#endif // QN_X11_IMAGES_H
