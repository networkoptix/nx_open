#include "images_win.h"

#include <QtGui/QGuiApplication>

// TODO: #Elric use public interface QtWin::imageFromHBITMAP
extern QImage qt_imageFromWinHBITMAP(HDC hdc, HBITMAP bitmap, int w, int h);

namespace {
    /* These pixmaps are copied from qwindowsdrag.cpp.
     * They are not available through QPlatformDrag public interface so it is not
     * possible to retrieve them without some truly evil hacks. */

    const char * const moveDragCursorXpmC[] = {
        "11 20 3 1",
        ".        c None",
        "a        c #FFFFFF",
        "X        c #000000", // X11 cursor is traditionally black
        "aa.........",
        "aXa........",
        "aXXa.......",
        "aXXXa......",
        "aXXXXa.....",
        "aXXXXXa....",
        "aXXXXXXa...",
        "aXXXXXXXa..",
        "aXXXXXXXXa.",
        "aXXXXXXXXXa",
        "aXXXXXXaaaa",
        "aXXXaXXa...",
        "aXXaaXXa...",
        "aXa..aXXa..",
        "aa...aXXa..",
        "a.....aXXa.",
        "......aXXa.",
        ".......aXXa",
        ".......aXXa",
        "........aa."};

    const char * const copyDragCursorXpmC[] = {
        "24 30 3 1",
        ".        c None",
        "a        c #000000",
        "X        c #FFFFFF",
        "XX......................",
        "XaX.....................",
        "XaaX....................",
        "XaaaX...................",
        "XaaaaX..................",
        "XaaaaaX.................",
        "XaaaaaaX................",
        "XaaaaaaaX...............",
        "XaaaaaaaaX..............",
        "XaaaaaaaaaX.............",
        "XaaaaaaXXXX.............",
        "XaaaXaaX................",
        "XaaXXaaX................",
        "XaX..XaaX...............",
        "XX...XaaX...............",
        "X.....XaaX..............",
        "......XaaX..............",
        ".......XaaX.............",
        ".......XaaX.............",
        "........XX...aaaaaaaaaaa",
        ".............aXXXXXXXXXa",
        ".............aXXXXXXXXXa",
        ".............aXXXXaXXXXa",
        ".............aXXXXaXXXXa",
        ".............aXXaaaaaXXa",
        ".............aXXXXaXXXXa",
        ".............aXXXXaXXXXa",
        ".............aXXXXXXXXXa",
        ".............aXXXXXXXXXa",
        ".............aaaaaaaaaaa"};

    const char * const linkDragCursorXpmC[] = {
        "24 30 3 1",
        ".        c None",
        "a        c #000000",
        "X        c #FFFFFF",
        "XX......................",
        "XaX.....................",
        "XaaX....................",
        "XaaaX...................",
        "XaaaaX..................",
        "XaaaaaX.................",
        "XaaaaaaX................",
        "XaaaaaaaX...............",
        "XaaaaaaaaX..............",
        "XaaaaaaaaaX.............",
        "XaaaaaaXXXX.............",
        "XaaaXaaX................",
        "XaaXXaaX................",
        "XaX..XaaX...............",
        "XX...XaaX...............",
        "X.....XaaX..............",
        "......XaaX..............",
        ".......XaaX.............",
        ".......XaaX.............",
        "........XX...aaaaaaaaaaa",
        ".............aXXXXXXXXXa",
        ".............aXXXaaaaXXa",
        ".............aXXXXaaaXXa",
        ".............aXXXaaaaXXa",
        ".............aXXaaaXaXXa",
        ".............aXXaaXXXXXa",
        ".............aXXaXXXXXXa",
        ".............aXXXaXXXXXa",
        ".............aXXXXXXXXXa",
        ".............aaaaaaaaaaa"};

    /**
     * The code is based on <tt>qt_pixmapFromWinHICON</tt> function from <tt>QtGui</tt>.
     *
     * Original code didn't return the icon's hotspot and contained a bug
     * in bitmap size estimation.
     */
    QPixmap qt_pixmapFromWinHICON(HICON icon, QPoint *hotSpot) {
        bool foundAlpha = false;
        HDC screenDevice = GetDC(0);
        HDC hdc = CreateCompatibleDC(screenDevice);
        ReleaseDC(0, screenDevice);

        ICONINFO iconinfo;
        BOOL status = GetIconInfo(icon, &iconinfo);
        if (!status)
            qWarning("QPixmap_fromWinHICON(), failed to GetIconInfo()");

        /* Original version did not support extracting the hotspot. */
        if(status && hotSpot) {
            hotSpot->setX(iconinfo.xHotspot);
            hotSpot->setY(iconinfo.yHotspot);
        }

        /* This is the width/height fix. */
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
        QImage image = qt_imageFromWinHBITMAP(hdc, winBitmap, w, h);

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
            QImage mask = qt_imageFromWinHBITMAP(hdc, winBitmap, w, h);

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

    LPCWSTR standardCursorName(Qt::CursorShape shape) {
        switch (shape) {
        case Qt::ArrowCursor:           return IDC_ARROW;
        case Qt::UpArrowCursor:         return IDC_UPARROW;
        case Qt::CrossCursor:           return IDC_CROSS;
        case Qt::WaitCursor:            return IDC_WAIT;
        case Qt::IBeamCursor:           return IDC_IBEAM;
        case Qt::SizeVerCursor:         return IDC_SIZENS;
        case Qt::SizeHorCursor:         return IDC_SIZEWE;
        case Qt::SizeBDiagCursor:       return IDC_SIZENESW;
        case Qt::SizeFDiagCursor:       return IDC_SIZENWSE;
        case Qt::SizeAllCursor:         return IDC_SIZEALL;
        case Qt::PointingHandCursor:    return IDC_HAND;
        case Qt::ForbiddenCursor:       return IDC_NO;
        case Qt::WhatsThisCursor:       return IDC_HELP;
        case Qt::BusyCursor:            return IDC_APPSTARTING;

        case Qt::BlankCursor:
        case Qt::SplitVCursor:
        case Qt::SplitHCursor:
        case Qt::OpenHandCursor:
        case Qt::ClosedHandCursor:
        case Qt::BitmapCursor:
            return NULL; /* These are hardcoded as pixmaps in Qt. */

        case Qt::DragCopyCursor:
        case Qt::DragMoveCursor:
        case Qt::DragLinkCursor:
            return NULL; /* These should be provided by QApplicationPrivate::getPixmapCursor, but are not... */

        default:
            return NULL;
        }
    }

} // anonymous namespace

QnWindowsImages::QnWindowsImages(QObject *parent):
    QnPlatformImages(parent)
{
    QGuiApplication* guiApp = qobject_cast<QGuiApplication*>(qApp);

    if (guiApp) {
        m_copyDragCursor = QCursor(QPixmap(copyDragCursorXpmC), 0, 0);
        m_moveDragCursor = QCursor(QPixmap(moveDragCursorXpmC), 0, 0);
        m_linkDragCursor = QCursor(QPixmap(linkDragCursorXpmC), 0, 0);
    } else {
        m_copyDragCursor = QCursor(Qt::DragCopyCursor);
        m_moveDragCursor = QCursor(Qt::DragMoveCursor);
        m_linkDragCursor = QCursor(Qt::DragLinkCursor);
    }
}

QCursor QnWindowsImages::bitmapCursor(Qt::CursorShape shape) const {
    LPCWSTR cursorName = standardCursorName(shape);
    if(cursorName) {
        HCURSOR handle = LoadCursorW(0, cursorName);

        QPoint hotSpot;
        QPixmap pixmap = qt_pixmapFromWinHICON(handle, &hotSpot);

        return QCursor(pixmap, hotSpot.x(), hotSpot.y());
    } else {
        switch (shape) {
        case Qt::DragCopyCursor:    return m_copyDragCursor;
        case Qt::DragMoveCursor:    return m_moveDragCursor;
        case Qt::DragLinkCursor:    return m_linkDragCursor;

        case Qt::BlankCursor:
        case Qt::SplitVCursor:
        case Qt::SplitHCursor:
        case Qt::OpenHandCursor:
        case Qt::ClosedHandCursor:
        case Qt::BitmapCursor:
            return QCursor(shape); // TODO

        default:
            return QCursor(shape);
        }
    }
}


