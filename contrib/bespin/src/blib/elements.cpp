
#include <qmath.h>
#include <QPainter>
#include <QImage>

#include "../makros.h"
#include "dpi.h"
#include "elements.h"

using namespace Bespin;

// #define fillRect(_X_,_Y_,_W_,_H_,_B_) setPen(Qt::NoPen); p.setBrush(_B_); p.drawRect(_X_,_Y_,_W_,_H_)
// #define fillRect2(_R_,_B_) setPen(Qt::NoPen); p.setBrush(_B_); p.drawRect(_R_)

#define DRAW_ROUND_RECT(_X_,_Y_,_W_,_H_,_RX_,_RY_) drawRoundedRect(QRectF(_X_, _Y_, _W_, _H_), _RX_, _RY_, Qt::RelativeSize)

#define SCALE(_N_) lround(_N_*ourScale)


#define EMPTY_PIX(_W_, _H_) \
QImage img(_W_,_H_, QImage::Format_ARGB32);\
img.fill(Qt::transparent);\
QPainter p(&img); p.setRenderHint(QPainter::Antialiasing);\
p.setPen(Qt::NoPen)

static QColor black = Qt::black;
#define SET_ALPHA(_A_) black.setAlpha(_A_); p.setBrush(black)
#define WHITE(_A_) QColor(255,255,255, _A_)
#define BLACK(_A_) QColor(0,0,0, _A_)

#if 0
void
Elements::renderButtonLight(Tile::Set &set)
{
   NEW_EMPTY_PIX(f9, f9);
   SET_ALPHA(30);
   p.drawRoundRect(0,0,f9,f9,90,90);
   SET_ALPHA(54);
   p.drawRoundRect(F(1),F(1),f9-2*F(1),f9-2*F(1),80,80);
   SET_ALPHA(64);
   p.drawRoundRect(F(2),F(2),f9-2*F(2),f9-2*F(2),70,70);
   SET_ALPHA(74);
   p.drawRoundRect(F(3),F(3),f9-2*F(3),f9-2*F(3),60,60);
   p.end();
   set = Tile::Set(*pix,f9_2,f9_2,f9-2*f9_2,f9-2*f9_2);
   set.setDefaultShape(Tile::Ring);
   delete pix;
}
#endif

static float ourShadowIntensity = 1.0;
void
Elements::setShadowIntensity(float intensity) { ourShadowIntensity = intensity; }
static float ourScale = 1.0;
void
Elements::setScale(float scale) { ourScale = scale; }

QImage
Elements::glow(int size, float width)
{
    EMPTY_PIX(size, size);
    const float d = size/2.0;
    const float w = width/size;
    QRadialGradient rg(d, d, d);
    rg.setColorAt(1.0-2.0*w, BLACK(0));
    rg.setColorAt(1.0-w, BLACK(192));
    rg.setColorAt(1.0, BLACK(0));
    p.fillRect(img.rect(), rg);
    p.end();
    return img;
}

QImage
Elements::shadow(int size, bool opaque, bool sunken, float factor)
{
    EMPTY_PIX(size, size);
    float d = size/2.0;
    QRadialGradient rg(d, d, d);
    const int alpha = (int) (ourShadowIntensity * factor * (sunken ? 70 : (opaque ? 48 : 20)));
    rg.setColorAt(0.7, BLACK(CLAMP(alpha,0,255)));
    rg.setColorAt(1.0, BLACK(0));
    p.fillRect(img.rect(), rg);
    p.end();
    return img;
}

QImage
Elements::roundMask(int size)
{
    EMPTY_PIX(size, size); p.setBrush(Qt::black);
    p.drawEllipse(img.rect()); p.end();
    return img;
}

QImage
Elements::roundedMask(int size, int factor)
{
    EMPTY_PIX(size, size); p.setBrush(Qt::black);
    p.drawRoundedRect(img.rect(), factor, factor, Qt::RelativeSize);
    p.end();
    return img;
}

QImage
Elements::sunkenShadow(int size, bool enabled)
{
    EMPTY_PIX(size, size);

    int add = enabled*30;
    const int add2 = lround(80./F(4));
    const int rAdd = lround(25./F(4));

    // draw a flat shadow
    SET_ALPHA(sqrt(ourShadowIntensity) * (55 + add));
    p.DRAW_ROUND_RECT(0, 0, size, size-F(2), 80, 80);

    // subtract light
    p.setCompositionMode( QPainter::CompositionMode_DestinationOut );
    add = 100 + 30 - add; int xOff;
    for (int i = 1; i <= F(4); ++i)
    {
        xOff = qMax(i-F(2),0);
        SET_ALPHA(add+i*add2);
        p.DRAW_ROUND_RECT(xOff, i, size-2*xOff, size-(F(2)+i), 75+rAdd, 75+rAdd);
    }

    // add bottom highlight
    p.setCompositionMode( QPainter::CompositionMode_SourceOver );
    p.fillRect(F(3),size-F(2),size-2*F(3),F(1), BLACK(7+3*enabled));
    int w = size/F(3);
    p.fillRect(w,size-F(1),size-2*w,F(1), WHITE(20+10*enabled));

    p.end();

    return img;
}

QImage
Elements::relief(int size, bool enabled)
{
    const float f = ourShadowIntensity * (enabled ? 1.0 : 0.7);
    EMPTY_PIX(size, size);
    p.setBrush(Qt::NoBrush);
    const float f1_2 = F(1)/2.0;
    p.setPen(QPen(BLACK(int(f*70)), F(1)));
    p.DRAW_ROUND_RECT(f1_2, f1_2, size-F(1), size-F(2), 99, 99);
    p.setPen(QPen(WHITE(int(f*35)), F(1)));
    p.DRAW_ROUND_RECT(f1_2, F(1)+f1_2, size-F(1), size-F(2), 99, 99);
    p.end();
    return img;
}

#define DRAW_ROUND_ALPHA_RECT(_A_, _X_, _Y_, _W_,_R_)\
p.setBrush(BLACK(_A_)); p.DRAW_ROUND_RECT(_X_, _Y_, _W_, ss, (_R_+1)/2, _R_)

QImage
Elements::groupShadow(int size)
{
    const int ss = 2*size;
    EMPTY_PIX(size, size);

    DRAW_ROUND_ALPHA_RECT(5, 0, 0, size, 48);
    DRAW_ROUND_ALPHA_RECT(9, F(1), F(1), size-F(2), 32);
    DRAW_ROUND_ALPHA_RECT(11, F(2), F(2), size-F(4), 20);
    DRAW_ROUND_ALPHA_RECT(13, F(3), F(3), size-F(6), 12);

    p.setCompositionMode( QPainter::CompositionMode_DestinationIn );
    p.setBrush(BLACK(0)); p.DRAW_ROUND_RECT(F(4), F(2), size-F(8), ss, 6, 11);

    p.setCompositionMode( QPainter::CompositionMode_SourceOver );
    p.setPen(WHITE(60));
    p.setBrush(Qt::NoBrush);
    p.DRAW_ROUND_RECT(F(4), F(2), size-F(8), ss, 6, 11);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setCompositionMode( QPainter::CompositionMode_DestinationIn );
    int f33 = SCALE(33);
    for (int i = 1; i < f33; ++i)
    {
        p.setPen(BLACK(CLAMP(i*lround(255.0/F(32)),0,255)));
        p.drawLine(0, size-i, size, size-i);
    }
    p.end();

    return img;
}

#if 0
void
Elements::renderLightLine(Tile::Line &line)
{
   int f49 = SCALE(49);
   int f49_2 = (f49-1)/2;
   NEW_EMPTY_PIX(f49,f49);
   QRadialGradient rg( pix->rect().center(), f49_2 );
   rg.setColorAt ( 0, WHITE(160) ); rg.setColorAt ( 1, WHITE(0) );
   p.fillRect(0,0,f49,f49,rg);
   p.end();
   QPixmap tmp = pix->scaled( f49, dpi.f5, Qt::IgnoreAspectRatio,
                              Qt::SmoothTransformation );
   tmp = tmp.copy(0,F(2),f49,F(3));
   line = Tile::Line(tmp,Qt::Horizontal,f49_2,-f49_2);
   delete pix;
}
#endif

#undef fillRect
