/* Bespin widget style for Qt4
   Copyright (C) 2007 Thomas Luebking <thomas.luebking@web.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
 */

#ifndef BESPIN_HACKS_H
#define BESPIN_HACKS_H

#include <QObject>

class QWidget;
class QScrollBar;

namespace Bespin
{

class Hacks : public QObject
{
    Q_OBJECT
public:
    Hacks() {}
    enum HackAppType { Unknown = 0, SMPlayer, Dragon, KDM, Gwenview, VLC };
    bool eventFilter( QObject *, QEvent *);
    static bool add(QWidget *w);
    static void releaseApp();
    static void remove(QWidget *w);
    static struct Config
    {
        bool messages, KHTMLView, treeViews, windowMovement, killThrobber, fixGwenview,
             opaqueDolphinViews, opaqueAmarokViews, opaquePlacesViews,
             lockToolBars, invertDolphinUrlBar, fixKMailFolderList, extendDolphinViews, lockDocks,
             konsoleScanlines, suspendFullscreenPlayers;
    } config;
private slots:
    void toggleToolBarLock();
    void fixGwenviewPosition();
private:
    Q_DISABLE_COPY(Hacks)
};
} // namespace
#endif // BESPIN_HACKS_H
