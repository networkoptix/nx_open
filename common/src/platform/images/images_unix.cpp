#include "images_unix.h"

#ifdef Q_WS_X11

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

QPixmap QnX11Images::cursorImageInt(QCursor &cursor) const {
    QApplication::setOverrideCursor(cursor);
    XFixesCursorImage *xfcursorImage = XFixesGetCursorImage( QX11Info::display() );
    QApplication::restoreOverrideCursor();

    QImage mouseCursor( (uchar*)xfcursorImage->pixels, xfcursorImage->width,xfcursorImage->height, QImage::Format_ARGB32_Premultiplied );

    QPixmap pixmapCursor;
    pixmapCursor.convertFromImage(mouseCursor);
    return pixmapCursor;
}

#endif
