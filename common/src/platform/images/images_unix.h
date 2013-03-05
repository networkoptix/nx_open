#ifndef QN_X11_IMAGES_H
#define QN_X11_IMAGES_H

#include <QtCore/QtGlobal>

#ifdef Q_WS_X11

#include "platform_images.h"
#include <QtGui/QPixmap>

class QnX11Images: public QnPlatformImages {
    Q_OBJECT

    typedef QnPlatformImages base_type;

public:
    QnX11Images(QObject *parent = NULL);
    virtual ~QnX11Images();

    virtual QPixmap cursorImage(Qt::CursorShape shape) const
    {
        QCursor tmp(shape);
        return cursorImageInt(tmp);
    }

private:
     QPixmap cursorImageInt(QCursor &cursor) const;
};

#endif // Q_WS_X11

#endif // QN_X11_IMAGES_H
