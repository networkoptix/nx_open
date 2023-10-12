// Modified by Network Optix, Inc. Original copyright notice follows.

/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtOpenVG module of the Qt Toolkit.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
****************************************************************************/

#include "utils_win.h"

/**
 * The code is based on <tt>qt_pixmapFromWinHICON</tt> function from <tt>QtGui</tt>.
 *
 * Original code didn't return the icon's hotspot and contained a bug
 * in bitmap size estimation.
 */
QPixmap pixmapFromHICON(HICON icon, QPoint* hotSpot)
{
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
    bool bwCursor = iconinfo.hbmColor == nullptr;
    BITMAP bmpinfo = {0};
    if (GetObject(iconinfo.hbmMask, sizeof(BITMAP), &bmpinfo) != 0) {
        w = bmpinfo.bmWidth;
        h = qAbs(bmpinfo.bmHeight) / (bwCursor ? 2 : 1);
    }

    BITMAPINFOHEADER bitmapInfo;
    bitmapInfo.biSize        = sizeof(BITMAPINFOHEADER);
    bitmapInfo.biWidth       = w;
    bitmapInfo.biHeight      = -h; //< Top to bottom.
    bitmapInfo.biPlanes      = 1;
    bitmapInfo.biBitCount    = 32;
    bitmapInfo.biCompression = BI_RGB;
    bitmapInfo.biSizeImage   = 0;
    bitmapInfo.biXPelsPerMeter = 0;
    bitmapInfo.biYPelsPerMeter = 0;
    bitmapInfo.biClrUsed       = 0;
    bitmapInfo.biClrImportant  = 0;
    DWORD* bits = nullptr;

    HBITMAP winBitmap = CreateDIBSection(hdc, (BITMAPINFO*)&bitmapInfo, DIB_RGB_COLORS, (VOID**)&bits, nullptr, 0);
    HGDIOBJ oldhdc = (HBITMAP)SelectObject(hdc, winBitmap);
    DrawIconEx(hdc, 0, 0, icon, w, h, 0, 0, DI_NORMAL);

    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    if (bits)
        memcpy(image.bits(), bits, image.sizeInBytes());

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
        QImage mask = QImage::fromHBITMAP(winBitmap);

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
        return nullptr; /* These are hardcoded as pixmaps in Qt. */

    case Qt::DragCopyCursor:
    case Qt::DragMoveCursor:
    case Qt::DragLinkCursor:
        return nullptr; /* These should be provided by QApplicationPrivate::getPixmapCursor, but are not... */

    default:
        return nullptr;
    }
}
