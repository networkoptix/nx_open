#include "generic_images.h"

#include <QtGui/QCursor>
#include <QtGui/QPixmap>

QnGenericImages::QnGenericImages(QObject *parent):
    base_type(parent) 
{
}

QnGenericImages::~QnGenericImages() {
    return;
}

QPixmap QnGenericImages::cursorImage(Qt::CursorShape shape) const {
    QCursor cursor(shape);
    return cursor.pixmap();
}

