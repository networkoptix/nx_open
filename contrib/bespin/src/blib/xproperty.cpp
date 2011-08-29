//////////////////////////////////////////////////////////////////////////////
//
// -------------------
// Bespin style & window decoration for KDE
// -------------------
// Copyright (c) 2008 Thomas Luebking <baghira-style@gmx.net>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//////////////////////////////////////////////////////////////////////////////

#include "xproperty.h"


using namespace Bespin;

#include <X11/Xatom.h>
#include <QX11Info>

BLIB_EXPORT Atom XProperty::winData = XInternAtom(QX11Info::display(), "BESPIN_WIN_DATA", False);
BLIB_EXPORT Atom XProperty::bgPics = XInternAtom(QX11Info::display(), "BESPIN_BG_PICS", False);
BLIB_EXPORT Atom XProperty::decoDim = XInternAtom(QX11Info::display(), "BESPIN_DECO_DIM", False);
BLIB_EXPORT Atom XProperty::pid = XInternAtom(QX11Info::display(), "_NET_WM_PID", False);
BLIB_EXPORT Atom XProperty::blurRegion = XInternAtom(QX11Info::display(), "_KDE_NET_WM_BLUR_BEHIND_REGION", False);
BLIB_EXPORT Atom XProperty::forceShadows = XInternAtom( QX11Info::display(), "_KDE_SHADOW_FORCE", False );
BLIB_EXPORT Atom XProperty::kwinShadow = XInternAtom( QX11Info::display(), "_KDE_NET_WM_SHADOW", False );
//     const char* const ShadowHelper::netWMForceShadowPropertyName( "_KDE_NET_WM_FORCE_SHADOW" );
//     const char* const ShadowHelper::netWMSkipShadowPropertyName( "_KDE_NET_WM_SKIP_SHADOW" );
BLIB_EXPORT Atom XProperty::bespinShadow[2] = { XInternAtom( QX11Info::display(), "BESPIN_SHADOW_SMALL", False ),
                                                XInternAtom( QX11Info::display(), "BESPIN_SHADOW_LARGE", False ) };
BLIB_EXPORT Atom XProperty::netSupported = XInternAtom( QX11Info::display(), "_NET_SUPPORTED", False );
BLIB_EXPORT Atom XProperty::blockCompositing = XInternAtom( QX11Info::display(), "_KDE_NET_WM_BLOCK_COMPOSITING", False );

void
XProperty::setAtom(WId window, Atom atom)
{
    const char *data = "1";
    XChangeProperty(QX11Info::display(), window, atom, XA_ATOM, 32, PropModeReplace, (uchar*)data, 1 );
}

unsigned long
XProperty::handleProperty(WId window, Atom atom, uchar **data, Type type, unsigned long n)
{
    int format = (type == LONG ? 32 : type);
    Atom xtype = (type == ATOM ? XA_ATOM : XA_CARDINAL);
    if (*data) // this is ok, internally used only
    {
        XChangeProperty(QX11Info::display(), window, atom, xtype, format, PropModeReplace, *data, n );
        XSync(QX11Info::display(), False);
        return 0;
    }
    int result, de; //dead end
    unsigned long nn, de2;
    int nmax = n ? n : 0xffffffff;
    result = XGetWindowProperty(QX11Info::display(), window, atom, 0L, nmax, False, xtype, &de2, &de, &nn, &de2, data);
    if (result != Success || *data == X::None || (n > 0 && n != nn))
        *data = NULL; // superflous?!?
    return nn;
}

void
XProperty::remove(WId window, Atom atom)
{
    XDeleteProperty(QX11Info::display(), window, atom);
}

#if 0

/* The below functions mangle 2 rbg (24bit) colors and a 2 bit hint into
a 32bit integer to be set as X11 property
Of course this is convulsive, but doesn't hurt for our purposes
::encode() is a bit trickier as it needs to decide whether the color values
should be rounded up or down like
x = qMin(lround(x/8.0),31) IS WRONG! as it would impact the hue and while
value manipulations are acceptable, hue values are NOT (this is a 8v stepping
per channel and as we're gonna create gradients out of the colors, black could
turn some kind of very dark red...)
Just trust and don't touch ;) (Yes future Thomas, this means YOU!)
======================================================================*/

#include <QtDebug>
uint
XProperty::encode(const QColor &bg, const QColor &fg, uint hint)
{
    int r,g,b; bg.getRgb(&r,&g,&b);
    int d = r%8 + g%8 + b%8;
    if (d > 10)
    {
        r = qMin(r+8, 255);
        g = qMin(g+8, 255);
        b = qMin(b+8, 255);
    }
    uint info = (((r >> 3) & 0x1f) << 27) | (((g >> 3) & 0x1f) << 22) | (((b >> 3) & 0x1f) << 17);

    fg.getRgb(&r,&g,&b);
    d = r%8 + g%8 + b%8;
    if (d > 10)
    {
        r = qMin(r+8, 255);
        g = qMin(g+8, 255);
        b = qMin(b+8, 255);
    }
    info |= (((r >> 3) & 0x1f) << 12) | (((g >> 3) & 0x1f) << 7) | (((b >> 3) & 0x1f) << 2) | hint & 3;
    return info;
}

void
XProperty::decode(uint info, QColor &bg, QColor &fg, uint &hint)
{
    bg.setRgb(  ((info >> 27) & 0x1f) << 3,
                ((info >> 22) & 0x1f) << 3,
                ((info >> 17) & 0x1f) << 3 );
    fg.setRgb(  ((info >> 12) & 0x1f) << 3,
                ((info >> 7) & 0x1f) << 3,
                ((info >> 2) & 0x1f) << 3 );
    hint = info & 3;
}
#endif
