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

    uchar* cursor_data;
    bool free_cursor_data;

    /* Like all X APIs, XFixesGetCursorImage() returns arrays of 32-bit
     * quantities as arrays of long; we need to convert on 64 bit */
    if (sizeof(long) == 4) {
        cursor_data = reinterpret_cast<uchar *>(xImage->pixels);
        free_cursor_data = false;
    }
    else
    {
        int i, j;
        quint32 *cursor_words;
        ulong *p;
        quint32 *q;

        cursor_words = new quint32[xImage->width * xImage->height];
        cursor_data = (uchar *)cursor_words;

        p = xImage->pixels;
        q = cursor_words;
        for (j = 0; j < xImage->height; j++)
            for (i = 0; i < xImage->width; i++)
                *(q++) = *(p++);

        free_cursor_data = true;
    }

    QCursor result(
        QPixmap::fromImage(QImage(cursor_data, xImage->width, xImage->height, QImage::Format_ARGB32_Premultiplied)),
        xImage->xhot,
        xImage->yhot
    );

    if (free_cursor_data)
        delete [] cursor_data;

    XFree(xImage);
    return result;
}

