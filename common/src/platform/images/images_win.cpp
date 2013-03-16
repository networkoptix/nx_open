#include "images_win.h"

#ifdef Q_OS_WIN

#include <Windows.h>

namespace {

    /**
     * This functions is an exact copy of <tt>qt_fromWinHBITMAP</tt>
     * from <tt>qpixmap_win.cpp</tt> of Qt 4.7.4.
     */
    QImage qt_fromWinHBITMAP(HDC hdc, HBITMAP bitmap, int w, int h) {
        BITMAPINFO bmi;
        memset(&bmi, 0, sizeof(bmi));
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = w;
        bmi.bmiHeader.biHeight      = -h;
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        bmi.bmiHeader.biSizeImage   = w * h * 4;

        QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
        if (image.isNull())
            return image;

        // Get bitmap bits
        uchar *data = (uchar *) qMalloc(bmi.bmiHeader.biSizeImage);

        if (GetDIBits(hdc, bitmap, 0, h, data, &bmi, DIB_RGB_COLORS)) {
            // Create image and copy data into image.
            for (int y=0; y<h; ++y) {
                void *dest = (void *) image.scanLine(y);
                void *src = data + y * image.bytesPerLine();
                memcpy(dest, src, image.bytesPerLine());
            }
        } else {
            qWarning("qt_fromWinHBITMAP(), failed to get bitmap bits");
        }
        qFree(data);

        return image;
    }

    /**
     * The code is copied from <tt>QPixmap::fromWinHICON</tt>. Original code contained 
     * a bug in bitmap size extimation, which is fixed in this version.
     */
    QPixmap QPixmap_fromWinHICON(HICON icon) {
        bool foundAlpha = false;
        HDC screenDevice = GetDC(0);
        HDC hdc = CreateCompatibleDC(screenDevice);
        ReleaseDC(0, screenDevice);

        ICONINFO iconinfo;
        bool result = GetIconInfo(icon, &iconinfo);
        if (!result)
            qWarning("QPixmap::fromWinHICON(), failed to GetIconInfo()");

        int w = 0, h = 0;
        bool bwCursor = iconinfo.hbmColor == NULL;
        BITMAP bmpinfo = {0};
        if (GetObject(iconinfo.hbmMask, sizeof(BITMAP), &bmpinfo) != 0) {
            w = bmpinfo.bmWidth;
            h = qAbs(bmpinfo.bmHeight) / (bwCursor ? 2 : 1);
        }

        BITMAPINFOHEADER bitmapInfo;
        bitmapInfo.biSize        = sizeof(BITMAPINFOHEADER);
        bitmapInfo.biWidth       = w;
        bitmapInfo.biHeight      = h;
        bitmapInfo.biPlanes      = 1;
        bitmapInfo.biBitCount    = 32;
        bitmapInfo.biCompression = BI_RGB;
        bitmapInfo.biSizeImage   = 0;
        bitmapInfo.biXPelsPerMeter = 0;
        bitmapInfo.biYPelsPerMeter = 0;
        bitmapInfo.biClrUsed       = 0;
        bitmapInfo.biClrImportant  = 0;
        DWORD* bits;

        HBITMAP winBitmap = CreateDIBSection(hdc, (BITMAPINFO*)&bitmapInfo, DIB_RGB_COLORS, (VOID**)&bits, NULL, 0);
        HGDIOBJ oldhdc = (HBITMAP)SelectObject(hdc, winBitmap);
        DrawIconEx( hdc, 0, 0, icon, w, h, 0, 0, DI_NORMAL);
        QImage image = qt_fromWinHBITMAP(hdc, winBitmap, w, h);

        for (int y = 0 ; y < h && !foundAlpha ; y++) {
            QRgb *scanLine= reinterpret_cast<QRgb *>(image.scanLine(y));
            for (int x = 0; x < w ; x++) {
                if (qAlpha(scanLine[x]) != 0) {
                    foundAlpha = true;
                    break;
                }
            }
        }
        if (!foundAlpha) {
            //If no alpha was found, we use the mask to set alpha values
            DrawIconEx( hdc, 0, 0, icon, w, h, 0, 0, DI_MASK);
            QImage mask = qt_fromWinHBITMAP(hdc, winBitmap, w, h);

            for (int y = 0 ; y < h ; y++){
                QRgb *scanlineImage = reinterpret_cast<QRgb *>(image.scanLine(y));
                QRgb *scanlineMask = mask.isNull() ? 0 : reinterpret_cast<QRgb *>(mask.scanLine(y));
                for (int x = 0; x < w ; x++){
                    if (scanlineMask && qRed(scanlineMask[x]) != 0)
                        scanlineImage[x] = 0; //mask out this pixel
                    else
                        scanlineImage[x] |= 0xff000000; // set the alpha channel to 255
                }
            }
        }
        
        //dispose resources created by iconinfo call
        DeleteObject(iconinfo.hbmMask);
        DeleteObject(iconinfo.hbmColor);

        SelectObject(hdc, oldhdc); //restore state
        DeleteObject(winBitmap);
        DeleteDC(hdc);
        return QPixmap::fromImage(image);
    }

} // anonymous namespace

QnWindowsImages::QnWindowsImages(QObject *parent):
    base_type(parent)
{
}

QnWindowsImages::~QnWindowsImages() {
    return;
}

QPixmap QnWindowsImages::cursorImage(Qt::CursorShape shape) const {

    QCursor cursor(shape);
    HCURSOR handle = cursor.handle();

    QPixmap result = QPixmap_fromWinHICON(handle);

    
    QDialog *d = new QDialog();
  
    QLabel *label = new QLabel();
    label->setPixmap(result);

    d->exec();

    /*PICONINFOEXW iconInfo;
    iconInfo.cbSize = sizeof(iconInfo);

    if(!GetIconInfoExW(handle, &iconInfo))
        return QPixmap();

    QPixmap result = QPixmap::fromWinHICON();

    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);*/

    return QPixmap();

#if 0
    HICON hicon;
    ICONINFO icInfo;

    CURSORINFO ci;
    ci.cbSize = sizeof(ci);

    if(GetCursorInfo(out ci))
    {
        if (ci.flags == Win32Stuff.CURSOR_SHOWING)
        { 
            hicon = Win32Stuff.CopyIcon(ci.hCursor);
            if(Win32Stuff.GetIconInfo(hicon, out icInfo))
            {
                x = ci.ptScreenPos.x - ((int)icInfo.xHotspot);
                y = ci.ptScreenPos.y - ((int)icInfo.yHotspot);
                Icon ic = Icon.FromHandle(hicon);
                bmp = ic.ToBitmap(); 

                return bmp;
            }
        }
    }
    return null;
#endif

#if 0
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
#endif
}


#endif
