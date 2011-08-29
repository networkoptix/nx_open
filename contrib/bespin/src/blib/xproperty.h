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

#ifndef XPROPERTY_H
#define XPROPERTY_H

//#include <QWidget>
//#include <QtDebug>

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include "fixx11h.h"

#include <QWidget>
#include <QtDebug>

namespace Bespin {


typedef struct _WindowData
{
    QRgb inactiveWindow, activeWindow, inactiveDeco, activeDeco,
         inactiveText, activeText, inactiveButton, activeButton;
    int style;
} WindowData;

typedef struct _WindowPics
{
    Picture topTile, btmTile, cnrTile, lCorner, rCorner;
} WindowPics;

class BLIB_EXPORT XProperty
{
public:
    enum Type { LONG = 1, BYTE = 8, WORD = 16, ATOM = 32 };
    static Atom winData, bgPics, decoDim, pid, blurRegion,
                forceShadows, kwinShadow, bespinShadow[2],
                netSupported, blockCompositing;

    template <typename T> inline static T *get(WId window, Atom atom, Type type, unsigned long *n = 0)
    {
        unsigned long nn = n ? *n : 1;
        T *data = 0;
        T **data_p = &data;
        nn = handleProperty(window, atom, (uchar**)data_p, type, _n<T>(type, nn));
        if (n)
            *n = nn;
        return data;
    }

    template <typename T> inline static void set(WId window, Atom atom, T *data, Type type, unsigned long n = 1)
    {
        if (!data) return;
        T **data_p = &data;
        handleProperty(window, atom, (uchar**)data_p, type, _n<T>(type, n));
    }

    static void remove(WId window, Atom atom);
    static void setAtom(WId window, Atom atom);
private:
    static unsigned long handleProperty(WId window, Atom atom, uchar **data, Type type, unsigned long n);
    template <typename T> inline static long _n(Type type, unsigned long n)
    {
        if (n < 1)
            return 0;
        unsigned long _n = n*sizeof(T)*8;
        _n /= (type == XProperty::LONG) ? 8*sizeof(long int) : (uint)type;
        return _n ? _n : 1L;
    }
};
}
#endif // XPROPERTY_H
