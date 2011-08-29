/* Bespin widget style for Qt4
   Copyright (C) 2011 Thomas Luebking <thomas.luebking@web.de>

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

#include <QEvent>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QX11Info>

#include <QtDebug>

#include "FX.h"
#include "shadows.h"
#include "tileset.h"
#ifdef Q_WS_X11
#include "xproperty.h"
#endif

using namespace Bespin;

#ifdef Q_WS_X11
class ShadowManager : public QObject {
public:
    ShadowManager() : QObject() {}
protected:
    bool eventFilter(QObject *o, QEvent *e)
    {
        if (e->type() == QEvent::Show)
        if (QWidget *w = qobject_cast<QWidget*>(o))
        if (w->isWindow() && w->testAttribute(Qt::WA_WState_Created) && w->internalWinId())
            Shadows::set(w->winId(), Shadows::Small);
        return false;
    }
};

static ShadowManager *shadowManager = 0;


static QPixmap (*pixmaps[2])[8] = {0,0};
static unsigned long globalShadowData[2][12];
#endif

static QPixmap nativePixmap(const QPixmap &qtPix)
{
#ifdef Q_WS_X11
    if (FX::usesXRender() || qtPix.isNull())
        return qtPix;

    Pixmap xPix = XCreatePixmap(QX11Info::display(), QX11Info::appRootWindow(), qtPix.width(), qtPix.height(), 32);
    QPixmap qtXPix(QPixmap::fromX11Pixmap( xPix, QPixmap::ExplicitlyShared ));
    QPainter p(&qtXPix);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawPixmap(0, 0, qtPix);
    p.end();
    return qtXPix;
#else
    return qtPix; // just for GCC - makes no sense at all anyway
#endif
}


static unsigned long*
shadowData(Shadows::Type t, bool storeToRoot)
{
#ifdef Q_WS_X11
    unsigned long _12 = 12;
    unsigned long *data = XProperty::get<unsigned long>(QX11Info::appRootWindow(), XProperty::bespinShadow[t-1], XProperty::LONG, &_12);
    if (!data)
    {
//         const int sz = (t == Shadows::Large) ? 32 : 20;
        int sz = 32;
        globalShadowData[t-1][8] = (sz-4)/2;
        globalShadowData[t-1][9] = 2*(sz-4)/3;
        globalShadowData[t-1][10] = sz-4;
        globalShadowData[t-1][11] = 2*(sz-4)/3;

        if (!pixmaps[t-1])
        {
            QPixmap *store = new QPixmap[8];
            pixmaps[t-1] = (QPixmap (*)[8])store;

            // radial gradient requires the raster engine anyway and we need *working* ... -> QImage
            QImage shadow(2*sz+1, 2*sz+1, QImage::Format_ARGB32);
            shadow.fill(Qt::transparent);

//             QRadialGradient rg(QPoint(sz+1,sz+1),sz);
            QRect shadowRect(shadow.rect());
            if (t == Shadows::Small)
            {
                sz = 22;
                shadowRect.adjust(8, 5, -8, -11);
            }
            QRadialGradient rg(shadowRect.center(), sz);

            QPainter p(&shadow);
            p.setPen(Qt::NoPen);

            rg.setColorAt(0, QColor(0,0,0,112-sz)); rg.setColorAt(0.98, QColor(0,0,0,0));
            p.setBrush(rg);
            p.drawRect(shadowRect);

            rg.setStops(QGradientStops());

            rg.setColorAt(0, QColor(0,0,0,96-sz)); rg.setColorAt(0.80, QColor(0,0,0,0));
            p.setBrush(rg);
            p.drawRect(shadowRect);

            rg.setStops(QGradientStops());

            rg.setColorAt(0, QColor(0,0,0,72-sz)); rg.setColorAt(0.66, QColor(0,0,0,0));
            p.setBrush(rg);
            p.drawRect(shadowRect);

            p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            p.setRenderHint(QPainter::Antialiasing);
            p.setBrush(Qt::transparent);
            p.drawRoundedRect(shadow.rect().adjusted(globalShadowData[t-1][9], globalShadowData[t-1][8],
                                                     -globalShadowData[t-1][11], -globalShadowData[t-1][10]), 8,8);
            p.end();

//             Tile::Set shadowSet(shadow,sz,sz,1,1);
            Tile::Set shadowSet(shadow,32,32,1,1);

            store[0] = nativePixmap(shadowSet.tile(Tile::TopMid));
            store[1] = nativePixmap(shadowSet.tile(Tile::TopRight));
            store[2] = nativePixmap(shadowSet.tile(Tile::MidRight));
            store[3] = nativePixmap(shadowSet.tile(Tile::BtmRight));
            store[4] = nativePixmap(shadowSet.tile(Tile::BtmMid));
            store[5] = nativePixmap(shadowSet.tile(Tile::BtmLeft));
            store[6] = nativePixmap(shadowSet.tile(Tile::MidLeft));
            store[7] = nativePixmap(shadowSet.tile(Tile::TopLeft));
        }
        for (int i = 0; i < 8; ++i)
            globalShadowData[t-1][i] = (*pixmaps[t-1])[i].handle();

        data = &globalShadowData[t-1][0];
        if (storeToRoot)
            XProperty::set(QX11Info::appRootWindow(), XProperty::bespinShadow[t-1], data, XProperty::LONG, 12);
    }
    return data;
#else
    return 0;  // just for GCC - makes no sense at all anyway
#endif
}

void
Shadows::cleanUp()
{
#ifdef Q_WS_X11
    delete shadowManager; shadowManager = 0;
    for (int i = 0; i < 2; ++i)
    {
        if (pixmaps[i])
        {
            if (!FX::usesXRender())
            {
                for (int j = 0; j < 8; ++j)
                    XFreePixmap(QX11Info::display(), (*pixmaps[i])[j].handle());
            }
            delete [] pixmaps[i];
            pixmaps[i] = 0L;
        }
    }
#endif
}

void
Shadows::manage(QWidget *w)
{
#ifdef Q_WS_X11
    if (!shadowManager)
        shadowManager = new ShadowManager;
    w->removeEventFilter(shadowManager);
    w->installEventFilter(shadowManager);
#endif
}

void
Shadows::set(WId id, Shadows::Type t, bool storeToRoot)
{
#ifdef Q_WS_X11
    if (id == QX11Info::appRootWindow()) {
        qWarning("BESPIN WARNING! Setting shadow to ROOT window is NOT supported");
        return;
    }
    switch(t)
    {
    case Shadows::None:
        XProperty::remove(id, XProperty::kwinShadow);
        break;
    case Shadows::Large:
    case Shadows::Small:
        XProperty::set(id, XProperty::kwinShadow, shadowData(t, storeToRoot), XProperty::LONG, 12);
    default:
        break;
    }
#endif
}

