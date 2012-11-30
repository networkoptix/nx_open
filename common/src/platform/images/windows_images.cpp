#include "windows_images.h"

#ifdef Q_OS_WIN

QnWindowsImages::QnWindowsImages(QObject *parent):
    base_type(parent)
{
}

QnWindowsImages::~QnWindowsImages() {
    return;
}

QPixmap QnWindowsImages::cursorImage(Qt::CursorShape shape) const {
    return QPixmap();
}

#endif
