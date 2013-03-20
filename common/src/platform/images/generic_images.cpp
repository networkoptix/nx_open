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

QCursor QnGenericImages::bitmapCursor(Qt::CursorShape shape) const {
    return QCursor(shape);
}

