#include "images_linux.h"

#include <QtGui/QCursor>
#include <QtGui/QPixmap>
#include <QtGui/QGuiApplication>
#include <QtX11Extras/QX11Info>

#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

/* X.h defines CursorShape, which conflicts with Qt::CursorShape. */
#undef CursorShape

QCursor QnLinuxImages::bitmapCursor(Qt::CursorShape shape) const {
    QGuiApplication::setOverrideCursor(shape);
    XFixesCursorImage *xImage = XFixesGetCursorImage(QX11Info::display());
    QGuiApplication::restoreOverrideCursor();
    if(!xImage)
        return QCursor(shape);

    uchar* cursorData;
    bool freeCursorData;

    /* Like all X APIs, XFixesGetCursorImage() returns arrays of 32-bit
     * quantities as arrays of long; we need to convert on 64 bit */
    if (sizeof(long) == 4) {
        cursorData = reinterpret_cast<uchar *>(xImage->pixels);
        freeCursorData = false;
    } else {
        int i, j;
        quint32 *cursorWords = new quint32[xImage->width * xImage->height];
        cursorData = (uchar *)cursorWords;

        ulong *p = xImage->pixels;
        quint32 *q = cursorWords;
        for (j = 0; j < xImage->height; j++)
            for (i = 0; i < xImage->width; i++)
                *(q++) = *(p++);
        freeCursorData = true;
    }

    QCursor result(
        QPixmap::fromImage(QImage(cursorData, xImage->width, xImage->height, QImage::Format_ARGB32)),
        xImage->xhot,
        xImage->yhot
    );

    if (freeCursorData)
        delete [] cursorData;

    XFree(xImage);
    return result;
}

