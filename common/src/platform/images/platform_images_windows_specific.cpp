
#ifdef _WIN32

#include "platform_images.h"

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
     * a bug in bitmap size estimation, which is fixed in this version. 
     */
    QPixmap QPixmap_fromWinHICON(HICON icon, QPoint *hotSpot) {
        bool foundAlpha = false;
        HDC screenDevice = GetDC(0);
        HDC hdc = CreateCompatibleDC(screenDevice);
        ReleaseDC(0, screenDevice);

        ICONINFO iconinfo;
        BOOL status = GetIconInfo(icon, &iconinfo);
        if (!status)
            qWarning("QPixmap_fromWinHICON(), failed to GetIconInfo()");

        if(status && hotSpot) {
            hotSpot->setX(iconinfo.xHotspot);
            hotSpot->setY(iconinfo.yHotspot);
        }

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
        DWORD *bits;

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


QCursor QnPlatformImages::bitmapCursor(Qt::CursorShape shape) const {
    QCursor cursor(shape);
    HCURSOR handle = cursor.handle();
    // TODO: There is a bug in Qt 4.7.4 that results in drag cursors returning NULL handle. We don't care.

    QPoint hotSpot;
    QPixmap pixmap = QPixmap_fromWinHICON(handle, &hotSpot);

    return QCursor(pixmap, hotSpot.x(), hotSpot.y());
}

#endif  //_WIN32
