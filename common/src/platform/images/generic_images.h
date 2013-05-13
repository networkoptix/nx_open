#ifndef QN_GENERIC_IMAGES_H
#define QN_GENERIC_IMAGES_H

#include "platform_images.h"

#include <QtGui/QPixmap>

/**
 * @brief The QnGenericImages class is dummy for platforms that are not Windows and not X11-based
 */
class QnGenericImages: public QnPlatformImages {
    Q_OBJECT

    typedef QnPlatformImages base_type;

public:
    QnGenericImages(QObject *parent = NULL);
    virtual ~QnGenericImages();

    virtual QCursor bitmapCursor(Qt::CursorShape shape) const override;
};


#endif // QN_GENERIC_IMAGES_H
