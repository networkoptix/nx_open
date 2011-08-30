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

#include <QPainter>

#ifdef Q_WS_X11
    #ifndef QT_NO_XRENDER
        #include <QX11Info>
        #include <QPaintEngine>
    #endif

#else
    #define QT_NO_XRENDER #
#endif


// #include <QPainter>
// #include <QWidget>
// #include <QtCore/QVarLengthArray>
#include <qmath.h>
#include <stdio.h>
#include "FX.h"

#ifdef Q_WS_X11
static Atom net_wm_cm;
#endif

using namespace FX;

#ifndef QT_NO_XRENDER

static Display *dpy = QX11Info::display();
static Window root = RootWindow (dpy, DefaultScreen (dpy));

static OXPicture
createFill(Display *dpy, const XRenderColor *c)
{
    XRenderPictureAttributes pa;
    OXPixmap pixmap = XCreatePixmap (dpy, root, 1, 1, 32);
    if (!pixmap)
        return X::None;
    pa.repeat = True;
    OXPicture fill = XRenderCreatePicture (dpy, pixmap, XRenderFindStandardFormat (dpy, PictStandardARGB32), CPRepeat, &pa);
    if (!fill)
    {
        XFreePixmap (dpy, pixmap);
        return X::None;
    }
    XRenderFillRectangle (dpy, PictOpSrc, fill, c, 0, 0, 1, 1);
    XFreePixmap (dpy, pixmap);
    return fill;
}

void
FX::freePicture(OXPicture pict)
{
    XRenderFreePicture (dpy, pict);
}

void
FX::composite(OXPicture src, OXPicture mask, OXPicture dst,
                    int sx, int sy, int mx, int my, int dx, int dy, uint w, uint h, int op)
{
    XRenderComposite (dpy, op, src, mask, dst, sx, sy, mx, my, dx, dy, w, h);
}

void
FX::composite(OXPicture src, OXPicture mask, const QPixmap &dst,
                    int sx, int sy, int mx, int my, int dx, int dy, uint w, uint h, int op)
{
   XRenderComposite (dpy, op, src, mask, dst.x11PictureHandle(), sx, sy, mx, my, dx, dy, w, h);
}

void
FX::composite(const QPixmap &src, OXPicture mask, const QPixmap &dst,
                    int sx, int sy, int mx, int my, int dx, int dy, uint w, uint h, int op)
{
    XRenderComposite(   dpy, op, src.x11PictureHandle(), mask,
                        dst.x11PictureHandle(), sx, sy, mx, my, dx, dy, w, h );
}

// adapted from Qt, because this really sucks ;)
void
FX::setColor(XRenderColor &xc, double r, double g, double b, double a)
{
    setColor(xc, QColor(r*0xff, g*0xff, b*0xff, a*0xff));
}

void
FX::setColor(XRenderColor &xc, QColor qc)
{
    uint a, r, g, b;
    qc.getRgb((int*)&r, (int*)&g, (int*)&b, (int*)&a);
    a = xc.alpha = (a | a << 8);
    xc.red   = (r | r << 8) * a / 0x10000;
    xc.green = (g | g << 8) * a / 0x10000;
    xc.blue  = (b | b << 8) * a / 0x10000;
}

#endif


/*
// Exponential blur, Jani Huhtanen, 2006 ==========================
*  expblur(QImage &img, int radius)
*
*  In-place blur of image 'img' with kernel of approximate radius 'radius'.
*  Blurs with two sided exponential impulse response.
*
*  aprec = precision of alpha parameter in fixed-point format 0.aprec
*  zprec = precision of state parameters zR,zG,zB and zA in fp format 8.zprec
*/

template<int aprec, int zprec>
static inline void blurinner(unsigned char *bptr, int &zR, int &zG, int &zB, int &zA, int alpha)
{
    int R,G,B,A;
    R = *bptr;
    G = *(bptr+1);
    B = *(bptr+2);
    A = *(bptr+3);

    zR += (alpha * ((R<<zprec)-zR))>>aprec;
    zG += (alpha * ((G<<zprec)-zG))>>aprec;
    zB += (alpha * ((B<<zprec)-zB))>>aprec;
    zA += (alpha * ((A<<zprec)-zA))>>aprec;

    *bptr =     zR>>zprec;
    *(bptr+1) = zG>>zprec;
    *(bptr+2) = zB>>zprec;
    *(bptr+3) = zA>>zprec;
}

template<int aprec,int zprec>
static inline void blurrow( QImage & im, int line, int alpha)
{
    int zR,zG,zB,zA;

    QRgb *ptr = (QRgb *)im.scanLine(line);

    zR = *((unsigned char *)ptr    )<<zprec;
    zG = *((unsigned char *)ptr + 1)<<zprec;
    zB = *((unsigned char *)ptr + 2)<<zprec;
    zA = *((unsigned char *)ptr + 3)<<zprec;

    for(int index=1; index<im.width(); index++)
        blurinner<aprec,zprec>((unsigned char *)&ptr[index],zR,zG,zB,zA,alpha);

    for(int index=im.width()-2; index>=0; index--)
        blurinner<aprec,zprec>((unsigned char *)&ptr[index],zR,zG,zB,zA,alpha);
}

template<int aprec, int zprec>
static inline void blurcol( QImage & im, int col, int alpha)
{
    int zR,zG,zB,zA;

    QRgb *ptr = (QRgb *)im.bits();
    ptr+=col;

    zR = *((unsigned char *)ptr    )<<zprec;
    zG = *((unsigned char *)ptr + 1)<<zprec;
    zB = *((unsigned char *)ptr + 2)<<zprec;
    zA = *((unsigned char *)ptr + 3)<<zprec;

    for(int index=im.width(); index<(im.height()-1)*im.width(); index+=im.width())
        blurinner<aprec,zprec>((unsigned char *)&ptr[index],zR,zG,zB,zA,alpha);

    for(int index=(im.height()-2)*im.width(); index>=0; index-=im.width())
        blurinner<aprec,zprec>((unsigned char *)&ptr[index],zR,zG,zB,zA,alpha);
}

void
FX::expblur( QImage &img, int radius )
{
    if(radius<1)
        return;

    static const int aprec = 16; static const int zprec = 7;

    // Calculate the alpha such that 90% of the kernel is within the radius. (Kernel extends to infinity)
    int alpha = (int)((1<<aprec)*(1.0f-expf(-2.3f/(radius+1.f))));

    for(int row=0;row<img.height();row++)
        blurrow<aprec,zprec>(img,row,alpha);

    for(int col=0;col<img.width();col++)
        blurcol<aprec,zprec>(img,col,alpha);
}
// ======================================================

// stolen from KWindowSystem
bool FX::compositingActive()
{
#ifdef Q_WS_X11
    return XGetSelectionOwner( QX11Info::display(), net_wm_cm ) != None;
#else
    return true;
#endif
}

static bool useRender = false;
static bool useRaster = false;

void
FX::init()
{
#ifdef Q_WS_X11
    Display *dpy = QX11Info::display();
    char string[ 100 ];
    sprintf(string, "_NET_WM_CM_S%d", DefaultScreen( dpy ));
    net_wm_cm = XInternAtom(dpy, string, False);
#endif
#ifndef QT_NO_XRENDER
    if (getenv("QT_X11_NO_XRENDER"))
        useRender = false;
    else
    {
        QPixmap pix(1,1);
        QPainter p(&pix);
        useRender = p.paintEngine()->type() == QPaintEngine::X11;
        useRaster = p.paintEngine()->type() == QPaintEngine::Raster;
        p.end();
    }
#endif
}

bool
FX::usesXRender()
{
    return useRender;
}

#ifndef QT_NO_XRENDER
static Picture _blendPicture = X::None;
static XRenderColor _blendColor = {0,0,0, 0xffff };
static Picture blendPicture(double opacity)
{
    _blendColor.alpha = ushort(opacity * 0xffff);
    if (_blendPicture == X::None)
        _blendPicture = createFill (dpy, &_blendColor);
    else
        XRenderFillRectangle(dpy, PictOpSrc, _blendPicture, &_blendColor, 0, 0, 1, 1);
    return _blendPicture;
}
#endif
#include <QtDebug>
bool
FX::blend(const QPixmap &upper, QPixmap &lower, double opacity, int x, int y)
{
    if (opacity == 0.0)
        return false; // haha...
#ifndef QT_NO_XRENDER
    if (useRender)
    {
        OXPicture alpha = (opacity == 1.0) ? 0 : blendPicture(opacity);
        XRenderComposite (dpy, PictOpOver, upper.x11PictureHandle(), alpha,
                          lower.x11PictureHandle(), 0, 0, 0, 0, x, y,
                          upper.width(), upper.height());
    }
    else
#endif
    {
        QPixmap tmp;
        if ( useRaster ) // raster engine is broken... :-(
        {
            tmp = QPixmap(upper.size());
            tmp.fill(Qt::transparent);
            QPainter p(&tmp);
            p.drawPixmap(0,0, upper);
            p.end();
        }
        else
            tmp = upper;

        QPainter p;
        if (opacity < 1.0)
        {
            p.begin(&tmp);
            p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            p.fillRect(tmp.rect(), QColor(0,0,0, opacity*255.0));
            p.end();
        }
        p.begin(&lower);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.drawPixmap(x, y, tmp);
        p.end();
    }
   return true;
}

QPixmap
FX::applyAlpha(const QPixmap &toThisPix, const QPixmap &fromThisPix, const QRect &rect, const QRect &alphaRect)
{
    QPixmap pix;
    int sx,sy,ax,ay,w,h;
    if (rect.isNull())
        { sx = sy = 0; w = toThisPix.width(); h = toThisPix.height(); }
    else
        rect.getRect(&sx,&sy,&w,&h);
    if (alphaRect.isNull())
        { ax = ay = 0; }
    else
    {
        ax = alphaRect.x(); ay = alphaRect.y();
        w = qMin(alphaRect.width(),w); h = qMin(alphaRect.height(),h);
    }

    if (w > fromThisPix.width() || h > fromThisPix.height())
        pix = QPixmap(w, h);
    else
        pix = fromThisPix.copy(0,0,w,h); // cause slow depth conversion...
    pix.fill(Qt::transparent);
#ifndef QT_NO_XRENDER
    if (useRender)
    {
        XRenderComposite( dpy, PictOpOver, toThisPix.x11PictureHandle(),
                          fromThisPix.x11PictureHandle(), pix.x11PictureHandle(),
                          sx, sy, ax, ay, 0, 0, w, h );
    }
    else
#endif
    {
        QPainter p(&pix);
        p.drawPixmap(0, 0, toThisPix, sx, sy, w, h);
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        p.drawPixmap(0, 0, fromThisPix, ax, ay, w, h);
        p.end();
    }
    return pix;
}

#if 1
// taken from QCommonStyle generatedIconPixmap - why oh why cannot KDE apps take this function into account...
static inline uint qt_intensity(uint r, uint g, uint b)
{
    // 30% red, 59% green, 11% blue
    return (77 * r + 150 * g + 28 * b) / 255;
}

void
FX::desaturate(QImage &img, const QColor &c)
{
    int r,g,b; c.getRgb(&r, &g, &b);
    uchar reds[256], greens[256], blues[256];

    for (int i=0; i<128; ++i)
    {
        reds[i]   = uchar((r * (i<<1)) >> 8);
        greens[i] = uchar((g * (i<<1)) >> 8);
        blues[i]  = uchar((b * (i<<1)) >> 8);
    }
    for (int i=0; i<128; ++i)
    {
        reds[i+128]   = uchar(qMin(r   + (i << 1), 255));
        greens[i+128] = uchar(qMin(g + (i << 1), 255));
        blues[i+128]  = uchar(qMin(b + (i << 1), 255));
    }

    int intensity = qt_intensity(r, g, b);
    const int f = 191;

    if ((r - f > g && r - f > b) || (g - f > r && g - f > b) || (b - f > r && b - f > g))
        intensity = qMin(255, intensity + 91);
    else if (intensity <= 128)
        intensity -= 51;

    for (int y = 0; y < img.height(); ++y)
    {
        QRgb *scanLine = (QRgb*)img.scanLine(y);
        for (int x = 0; x < img.width(); ++x)
        {
            QRgb pixel = *scanLine;
            uint ci = uint(qGray(pixel)/3 + (130 - intensity / 3));
            *scanLine = qRgba(reds[ci], greens[ci], blues[ci], qAlpha(pixel));
            ++scanLine;
        }
    }
}
#endif
QPixmap
FX::tint(const QPixmap &mask, const QColor &color)
{
    QPixmap pix = mask.copy();
    pix.fill(Qt::transparent);

#ifndef QT_NO_XRENDER
    if (useRender)
    {
        Q2XRenderColor(c, color);
        OXPicture tnt = createFill (dpy, &c);
        if (tnt == X::None)
            return pix;

        XRenderComposite( dpy, PictOpOver, tnt, mask.x11PictureHandle(), pix.x11PictureHandle(),
                          0, 0, 0, 0, 0, 0, mask.width(), mask.height());
        XRenderFreePicture (dpy, tnt);
    }
    else
#endif
    {
        QPainter p(&pix);
        p.setPen(Qt::NoPen); p.setBrush(color);
        p.drawRect(pix.rect());
        p.end();
        pix = FX::applyAlpha(pix, mask);
    }

    return pix;
}


QPixmap
FX::fade(const QPixmap &pix, double percent)
{
    QPixmap newPix(pix.size());
    newPix.fill(Qt::transparent);
    blend(pix, newPix, percent);
    return newPix;
}

#if 0
void // TODO: would be cool to get this working - doesn't, though...
FX::setAlpha(QPixmap &pix, const OXPicture &alpha)
{
   XRenderPictureAttributes pa;
   pa.alpha_map = alpha;
   pa.alpha_x_origin = pa.alpha_y_origin = 0;
   XRenderChangePicture(dpy, pix.x11PictureHandle(), CPAlphaMap|CPAlphaXOrigin|CPAlphaYOrigin, &pa);
}
#endif

# if 0
/* "qt_getClipRects" is friend enough to qregion... ;)*/
#include <QRegion>
inline void *
qt_getClipRects( const QRegion &r, int &num )
{
    return r.clipRectangles( num );
}

void FX::setGradient(XLinearGradient &lg, QPoint p1, QPoint p2) {
   lg.p1.x = p1.x(); lg.p1.y = p1.y();
   lg.p2.x = p2.x(); lg.p2.y = p2.y();
}

void FX::setGradient(XLinearGradient &lg,
                           XFixed x1, XFixed y1, XFixed x2, XFixed y2) {
   lg.p1.x = x1; lg.p1.y = y1;
   lg.p2.x = x2; lg.p2.y = y2;
}

OXPicture FX::gradient(const QPoint start, const QPoint stop,
                             const ColorArray &colors,
                             const PointArray &stops) {
   XLinearGradient lg = {
      { start.x() << 16, start.y() << 16 },
      { stop.x() << 16, stop.y() << 16} };
   QVarLengthArray<XRenderColor> cs(colors.size());
   for (int i = 0; i < colors.size(); ++i)
      setColor(cs[i], colors.at(i));
   XFixed *stps;
   if (stops.size() < 2) {
      stps = new XFixed[2];
      stps[0] = 0; stps[1] = (1<<16);
   }
   else {
      int d = (1<<16);
      stps = new XFixed[stops.size()];
      for (int i = 0; i < stops.size(); ++i) {
         if (stops.at(i) < 0) continue;
         if (stops.at(i) > 1) break;
         stps[i] = stops.at(i)*d;
      }
   }
   XFlush (dpy);
   OXPicture lgp = XRenderCreateLinearGradient(dpy, &lg, stps, &cs[0],
                                               qMin(qMax(stops.size(),2),
                                                  colors.size()));
   delete[] stps;
   return lgp;
}

OXPicture FX::gradient(const QPoint c1, int r1, const QPoint c2, int r2,
                             const ColorArray &colors,
                             const PointArray &stops) {
   XRadialGradient rg = {
      { c1.x() << 16, c1.y() << 16, r1 << 16 },
      { c2.x() << 16, c2.y() << 16, r2 << 16 } };
   QVarLengthArray<XRenderColor> cs(colors.size());
   for (int i = 0; i < colors.size(); ++i)
      setColor(cs[i], colors.at(i));
   XFixed *stps;
   if (stops.size() < 2) {
      stps = new XFixed[2];
      stps[0] = 0; stps[1] = (1<<16);
   }
   else {
      int d = ((int)(sqrt(pow(c2.x()-c1.x(),2)+pow(c2.y()-c1.y(),2)))) << 16;
      stps = new XFixed[stops.size()];
      for (int i = 0; i < stops.size(); ++i)
      {
         if (stops.at(i) < 0) continue;
         if (stops.at(i) > 1) break;
         stps[i] = stops.at(i)*d;
      }
   }
   XFlush (dpy);
   OXPicture lgp = XRenderCreateRadialGradient(dpy, &rg, stps, &cs[0],
                                               qMin(qMax(stops.size(),2),
                                                  colors.size()));
   delete[] stps;
   return lgp;
}
#endif

