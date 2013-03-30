#include "images_unix.h"

#include <QtGui/QCursor>
#include <QtGui/QPixmap>
#include <QtGui/QX11Info>
#include <QtGui/QApplication>

#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

/* X.h defines CursorShape, which conflicts with Qt::CursorShape. */
#undef CursorShape

QnX11Images::QnX11Images(QObject *parent):
    base_type(parent) 
{
}

QnX11Images::~QnX11Images() {
    return;
}

QCursor QnX11Images::bitmapCursor(Qt::CursorShape shape) const {
    QApplication::setOverrideCursor(shape);
    XFixesCursorImage *xImage = XFixesGetCursorImage(QX11Info::display());
    QApplication::restoreOverrideCursor();
    if(!xImage)
        return QCursor(QPixmap());

    uchar* cursorData;
    bool freeCursorData;

    /* Like all X APIs, XFixesGetCursorImage() returns arrays of 32-bit
     * quantities as arrays of long; we need to convert on 64 bit */
    if (sizeof(long) == 4) {
        cursorData = reinterpret_cast<uchar *>(xImage->pixels);
        freeCursorData = false;
    }
    else
    {
        int i, j;
        quint32 *cursorWords;
        ulong *p;
        quint32 *q;

        cursorWords = new quint32[xImage->width * xImage->height];
        cursorData = (uchar *)cursorWords;

        p = xImage->pixels;
        q = cursorWords;
        for (j = 0; j < xImage->height; j++)
            for (i = 0; i < xImage->width; i++)
                *(q++) = *(p++);

        freeCursorData = true;
    }

    QCursor result(
        QPixmap::fromImage(QImage(cursorData, xImage->width, xImage->height, QImage::Format_ARGB32_Premultiplied)),
        xImage->xhot,
        xImage->yhot
    );

    if (freeCursorData)
        delete [] cursorData;

    XFree(xImage);
    return result;
}

