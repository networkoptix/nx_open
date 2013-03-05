#include "images_win.h"

#include <windows.h>

#ifdef Q_OS_WIN

QnWindowsImages::QnWindowsImages(QObject *parent):
    base_type(parent)
{
}

QnWindowsImages::~QnWindowsImages() {
    return;
}

QPixmap QnWindowsImages::cursorImage(Qt::CursorShape shape) const {

    if (shape == Qt::WhatsThisCursor) {
        QCursor cursor(shape);
        return QPixmap::fromWinHICON(cursor.handle());
    }

    HBITMAP hbitmap;
    BITMAP bitmap;
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    CURSORINFO cursorInfo = { 0 };
    cursorInfo.cbSize = sizeof(cursorInfo);

    GetCursorInfo(&cursorInfo);

    ICONINFO ii = {0};
    GetIconInfo(cursorInfo.hCursor, &ii);

    hbitmap = ii.hbmColor;

    SelectObject(hdcMem, hbitmap);
    GetObject(hbitmap, sizeof(BITMAP), &bitmap);


    int w = bitmap.bmWidth;
    int h = bitmap.bmHeight;

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage   = w * h * 4;
    uchar *data = (uchar *) qMalloc(bmi.bmiHeader.biSizeImage);

    GetDIBits(hdcMem, hbitmap, 0, h, data, &bmi, DIB_RGB_COLORS);
    QImage::Format imageFormat = QImage::Format_ARGB32_Premultiplied;
    uint mask = 0;
//    if (format == NoAlpha) {
        imageFormat = QImage::Format_RGB32;
        mask = 0xff000000;
//    }

    QImage result;
    // Create image and copy data into image.
    QImage image(w, h, imageFormat);
    if (!image.isNull()) { // failed to alloc?
        int bytes_per_line = w * sizeof(QRgb);
        for (int y=0; y<h; ++y) {
            QRgb *dest = (QRgb *) image.scanLine(y);
            const QRgb *src = (const QRgb *) (data + y * bytes_per_line);
            for (int x=0; x<w; ++x) {
                const uint pixel = src[x];
                if ((pixel & 0xff000000) == 0 && (pixel & 0x00ffffff) != 0)
                    dest[x] = pixel | 0xff000000;
                else
                    dest[x] = pixel | mask;
            }
        }
    }
    result = image;
    ReleaseDC(0, hdcScreen);

    return QPixmap::fromImage(result);

    //QCursor cursor(shape);
   // return QPixmap::fromWinHICON(cursor.handle());
//     return QPixmap::fromWinHBITMAP(hbitmap);
}


#endif
