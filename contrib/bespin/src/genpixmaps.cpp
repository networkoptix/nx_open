
#include <qmath.h>
#include <QPainter>
#include <QPixmap>

#include "blib/elements.h"
#include "bespin.h"
#include "makros.h"

using namespace Bespin;

static QColor black = Qt::black;
static float scale = 0.0, intensity = 0.0;
#define SET_ALPHA(_A_) black.setAlpha(_A_); p.setBrush(black)
#define WHITE(_A_) QColor(255,255,255, _A_)
#define BLACK(_A_) QColor(0,0,0, _A_)
#define SCALE(_N_) lround(_N_*config.scale)


void
Style::generatePixmaps()
{
    // interestingly every kde application creates _2_ style instances... OUCH!
    if (config.scale == scale && intensity == config.shadowIntensity)
        return;

    intensity = config.shadowIntensity;
    scale = config.scale;

    Elements::setShadowIntensity( config.shadowIntensity );

    const int f9 = F(9); const int f11 = SCALE(11);
    const int f13 = SCALE(13); const int f17 = SCALE(17);
    const int f49 = SCALE(49);

    // MASKS =======================================
    for (int i = 0; i < 2; ++i)
    {
        int s,r;
        if (i)
            { s = f13; r = 99; }
        else
            { s = f9; r = 70; }
        masks.rect[i] = Tile::Set(Elements::roundedMask(s, r),s/2,s/2,1,1, r);
        masks.rect[i].sharpenEdges();
    }
    masks.windowShape = Tile::Set(Elements::roundedMask(9, 99),4,4,1,1, 99);

    // SHADOWS ===============================
    // sunken
    for (int r = 0; r < 2; ++r)
    {
        int s = r ? f17 : f9;
        for (int i = 0; i < 2; ++i)
        {
            shadows.sunken[r][i] = Tile::Set(Elements::sunkenShadow(s, i), s/2,s/2,1,1);
            shadows.sunken[r][i].setDefaultShape(Tile::Ring);
        }
    }

    // relief
    for (int r = 0; r < 2; ++r)
    {
        int s = r ? f17 : f11;
        for (int i = 0; i < 2; ++i)
        {
            shadows.relief[r][i] = Tile::Set(Elements::relief(s, i), s/2,s/2,1,1);
            shadows.relief[r][i].setDefaultShape(Tile::Ring);
        }
    }

    // raised
    for (int r = 0; r < 2; ++r)
    {
        int s;  float f = .8;
        s =  r ? f17 : f9;
        for (int i = 0; i < 2; ++i) // opaque?
            for (int j = 0; j < 2; ++j)
            {   // sunken?
                shadows.raised[r][i][j] = Tile::Set(Elements::shadow(s,i,j,f), s/2, s/2, 1, 1);
                shadows.raised[r][i][j].setDefaultShape(Tile::Ring);
            }
    }


    // fallback ( sunken ) // TODO: raised
    QImage tmp(f9, f9, QImage::Format_ARGB32);
    QPainter p;
    p.begin(&tmp);
    p.fillRect(F(1),0,f9-F(2),F(1), BLACK(10));
    p.fillRect(F(2),F(1),f9-F(4),F(1), BLACK(20));
    p.fillRect(F(2),F(2),f9-F(4),F(1), BLACK(40));
    p.fillRect(F(3),F(3),f9-F(6),F(1), BLACK(80));

    p.fillRect(F(1),f9-F(1),f9-F(2),F(1), WHITE(10));
    p.fillRect(F(2),f9-F(2),f9-F(4),F(1), WHITE(20));
    p.fillRect(F(2),f9-F(3),f9-F(4),F(1), WHITE(40));
    p.fillRect(F(3),f9-F(4),f9-F(6),F(1), WHITE(80));

    p.fillRect(0,F(1),F(1),f9-F(2), QColor(128,128,128,10));
    p.fillRect(F(1),F(2),F(1),f9-F(4), QColor(128,128,128,20));
    p.fillRect(F(2),F(2),F(1),f9-F(4), QColor(128,128,128,40));
    p.fillRect(F(3),F(3),F(1),f9-F(6), QColor(128,128,128,80));

    p.fillRect(f9-F(1),F(1),F(1),f9-F(2), QColor(128,128,128,10));
    p.fillRect(f9-F(2),F(2),F(1),f9-F(4), QColor(128,128,128,20));
    p.fillRect(f9-F(3),F(2),F(1),f9-F(4), QColor(128,128,128,40));
    p.fillRect(f9-F(4),F(3),F(1),f9-F(6), QColor(128,128,128,80));

    p.end();
    shadows.fallback = Tile::Set(tmp,f9/2,f9/2,1,1);
    shadows.fallback.setDefaultShape(Tile::Ring);
    // ================================================================

    // LIGHTS ==================================
    for (int r = 0; r < 2; ++r)
    {
        int s = r ? f17 : f11;
        lights.rect[r] = Tile::Set(Elements::shadow(s, true, false, 3.0), s/2,s/2,1,1);
        lights.rect[r].setDefaultShape(Tile::Ring);
    }

    for (int r = 0; r < 2; ++r)
    {
        int s = r ? f17 : f9;
        lights.glow[r] = Tile::Set(Elements::glow(s, 2.25*config.scale), s/2,s/2,1,1);
        lights.glow[r].setDefaultShape(Tile::Ring);
    }

   // toplight -- UNUSED!
//    renderLightLine(lights.top);

    // ================================================================

    // SLIDER =====================================
    // shadow
    for (int i = 0; i < 2; ++i)
    {   // opaque?
        shadows.slider[i][false] = QPixmap::fromImage(Elements::shadow(Dpi::target.SliderControl, i, false));
        shadows.slider[i][true] = QPixmap::fromImage(Elements::shadow(Dpi::target.SliderControl-F(2), i, true));
    }
    lights.slider = QPixmap::fromImage(Elements::shadow(Dpi::target.SliderControl, true, false, 3.0));
    masks.slider = QPixmap::fromImage(Elements::roundMask(Dpi::target.SliderControl-F(4)));
    // ================================================================

    // RADIOUTTON =====================================
    // shadow
    for (int i = 0; i < 2; ++i)
    {   // opaque?
        shadows.radio[i][false] = QPixmap::fromImage(Elements::shadow(Dpi::target.ExclusiveIndicator, i,false));
        shadows.radio[i][true] = QPixmap::fromImage(Elements::shadow(Dpi::target.ExclusiveIndicator-F(2), i,true));
    }
    // mask
    masks.radio = QPixmap::fromImage(Elements::roundMask(Dpi::target.ExclusiveIndicator-F(4)));
    // mask fill
#if 0
    masks.radioIndicator = roundMask(Dpi::target.ExclusiveIndicator - (config.btn.layer ? dpi.f10 : dpi.f12));
#else
    int s = (Dpi::target.ExclusiveIndicator)/5;
    s *= 2; // cause of int div...
    s += F(2); // cause sunken frame "outer" part covers F(2) pixels
    masks.radioIndicator = QPixmap::fromImage(Elements::roundMask(Dpi::target.ExclusiveIndicator - s));
#endif
    // ================================================================
    // NOTCH =====================================
    masks.notch = QPixmap::fromImage(Elements::roundMask(F(6)));
    // ================================================================


// GROUPBOX =====================================
    // shadow
    shadows.group = Tile::Set(Elements::groupShadow(f49),F(12),F(12),f49-2*F(12),F(1));
    shadows.group.setDefaultShape(Tile::Ring);
    // ================================================================

    // LINES =============================================
    int f49_2 = (f49-1)/2;
    QLinearGradient lg; QGradientStops stops;
    int w,h,c1,c2;
    for (int i = 0; i < 2; ++i)
    {   // orientarion
        if (i)
        {
            w = F(2); h = f49;
            lg = QLinearGradient(0,0,0,f49);
        }
        else
        {
            w = f49; h = F(2);
            lg = QLinearGradient(0,0,f49,0);
        }
        tmp = QImage(w,h,QImage::Format_ARGB32);
        for (int j = 0; j < 3; ++j)
        {   // direction
            c1 = 255*(j > 0);
            c2 = 255-c1;
            tmp.fill(Qt::transparent); p.begin(&tmp);

            stops   << QGradientStop( 0, QColor(c1,c1,c1,0) )
                    << QGradientStop( 0.5, QColor(c1,c1,c1,16) )
                    << QGradientStop( 1, QColor(c1,c1,c1,0) );
            lg.setStops(stops);
            QRect r;
            if (i)
            {
                r.setRect(0,0,F(1),f49);
                p.setClipRect(r);
                p.fillRect(r,lg);
                r.setRect(F(1),0,F(2)-F(1),f49);
            }
            else
            {
                r.setRect(0,0,f49,F(1));
                p.setClipRect(r);
                p.fillRect(r,lg);
                r.setRect(0,F(1),f49,F(2)-F(1));
            }

            stops.clear();
            stops   << QGradientStop( 0, QColor(c2,c2,c2,0) )
                    << QGradientStop( 0.5, QColor(c2,c2,c2,16) )
                    << QGradientStop( 1, QColor(c2,c2,c2,0) );
            lg.setStops(stops);

            p.setClipRect( r );
            p.fillRect(r,lg);

            stops.clear();
            p.end();
            shadows.line[i][j] = Tile::Line(QPixmap::fromImage(tmp), i ? Qt::Vertical : Qt::Horizontal, f49_2, -f49_2);
        }
    }
    // ================================================================

    // ================================================================
    // Popup corners - not really pxmaps, though ;) ===================
    // they at least break beryl's popup shadows...
    // see bespin.cpp#Style::eventfilter as well
    int f5 = 4;
    QBitmap bm(2*f5, 2*f5);
    bm.fill(Qt::black);
    p.begin(&bm);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawEllipse(0,0,2*f5,2*f5);
    p.end();
#if 0
    QRegion circle(bm);
    masks.corner[0] = circle & QRegion(0,0,f5,f5); // tl
    masks.corner[1] = circle & QRegion(f5,0,f5,f5); // tr
    masks.corner[1].translate(-masks.corner[1].boundingRect().left(), 0);
    masks.corner[2] = circle & QRegion(0,f5,f5,f5); // bl
    masks.corner[2].translate(0, -masks.corner[2].boundingRect().top());
    masks.corner[3] = circle & QRegion(f5,f5,f5,f5); // br
    masks.corner[3].translate(-masks.corner[3].boundingRect().topLeft());
#endif
    // ================================================================
}
#undef fillRect
