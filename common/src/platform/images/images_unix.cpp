#include "images_unix.h"

#include <QtGui/QCursor>
#include <QtGui/QPixmap>
#include <QtGui/QX11Info>
#include <QtGui/QApplication>

#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

QnX11Images::QnX11Images(QObject *parent):
    base_type(parent) 
{
}

QnX11Images::~QnX11Images() {
    return;
}

QCursor QnX11Images::bitmapCursor(Qt::CursorShape shape) const {
    QApplication::setOverrideCursor(cursor);
    XFixesCursorImage *xImage = XFixesGetCursorImage(QX11Info::display());
    QApplication::restoreOverrideCursor();
    if(!xImage)
        return QCursor(QPixmap());

    // TODO: #GDM xCursorImage->pixels is unsigned long *, and sizeof(unsigned long) is 8 on x64, but RGBA is always 4 bytes. 
    // I'm somewhat confused about this, so please re-test that it works on x64.
    QCursor result(
        QPixmap::fromImage(QImage(reinterpret_cast<uchar *>(xImage->pixels), xImage->width, xImage->height, QImage::Format_ARGB32_Premultiplied)), 
        xImage->xhot,
        xImage->yhot
    );

    XFree(xImage);
    return result;
}

