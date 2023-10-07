// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "images_win.h"

#include <QtGui/QGuiApplication>

#include <qt_images/drag_cursor_xpm_win.h>
#include <qt_images/utils_win.h>

QnWindowsImages::QnWindowsImages(QObject *parent):
    QnPlatformImages(parent)
{
    QGuiApplication* guiApp = qobject_cast<QGuiApplication*>(qApp);

    if (guiApp) {
        m_copyDragCursor = QCursor(DragCursorXpm::copyDragCursor(), 0, 0);
        m_moveDragCursor = QCursor(DragCursorXpm::moveDragCursor(), 0, 0);
        m_linkDragCursor = QCursor(DragCursorXpm::linkDragCursor(), 0, 0);
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
        QPixmap pixmap = pixmapFromHICON(handle, &hotSpot);

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
