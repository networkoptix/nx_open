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

#include "drag_cursor_xpm_win.h"

namespace {

/* These pixmaps are copied from qwindowsdrag.cpp.
 * They are not available through QPlatformDrag public interface. */

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

}

QPixmap DragCursorXpm::moveDragCursor()
{
    return QPixmap(moveDragCursorXpmC);
}

QPixmap DragCursorXpm::copyDragCursor()
{
    return QPixmap(copyDragCursorXpmC);
}

QPixmap DragCursorXpm::linkDragCursor()
{
    return QPixmap(linkDragCursorXpmC);
}
