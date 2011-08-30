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

#include <QAbstractItemView>
#include "draw.h"
#include "animator/aprogress.h"

#define NEW_PROGRESS 0

static int step = -1;

void
Style::drawCapacityBar(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    ASSURE_OPTION(cb, ProgressBar);
    if (cb->maximum == cb->minimum)
        return;
        
    OPT_ENABLED

    const int f2 = F(2);
    QRect r = RECT;
    QPalette::ColorRole bg = widget ? widget->backgroundRole() : QPalette::Window;
    QPalette::ColorRole fg = bg;
    
    if (RECT.height() < F(16))
    {
        fg = widget ? widget->foregroundRole() : QPalette::WindowText;
        int w = r.width()*cb->progress/(cb->maximum - cb->minimum);
        masks.rect[false].render(RECT, painter, COLOR(fg));
        r.adjust(1, 1, -1, -1);
        if (cb->direction == Qt::LeftToRight)
            r.setRight(r.left() + w);
        else
            r.setLeft(r.right() - w);
        masks.rect[false].render(r, painter, COLOR(bg));
        return;
    }

    r.setBottom(r.bottom()-f2);
    masks.rect[false].render(r, painter, Gradients::Sunken, Qt::Vertical, Colors::mid(COLOR(fg), Qt::black,6, 1)); // CCOLOR(progress, Bg)
    shadows.sunken[false][isEnabled].render(RECT, painter);

    int w = r.width()*cb->progress/(cb->maximum - cb->minimum)  - f2;
    if (w > F(4))
    {
        if (cb->direction == Qt::LeftToRight)
            r.setLeft(r.right() - w);
        else
            r.setRight(r.left() + w);
        shadows.raised[false][isEnabled][false].render(r, painter);
        r.adjust(f2, f2, -f2, -f2);
#if 1
    if (widget && config.bg.opacity == 0xff)
    {
        QPixmap buffer(widget->size());
        QPainter bp(&buffer);
        erase(option, &bp, widget);
        bp.end();
        masks.rect[false].render(r, painter, buffer, r.topLeft());
    }
    else
        masks.rect[false].render(r, painter, FCOLOR(Window));
#else
        masks.rect[false].render(r, painter, GRAD(progress) == Gradients::Sunken ?
                                                               Gradients::Button :
                                                               GRAD(progress), Qt::Vertical, COLOR(fg));
#endif
    }
    else
        r = QRect();

    if (cb->textVisible && !cb->text.isEmpty())
    {
        QRect tr = painter->boundingRect(RECT, Qt::TextSingleLine | cb->textAlignment, cb->text);
        if (tr.width() <= RECT.width() - w)
        {   // paint on free part
            tr = RECT;
            if (r.isValid())
            {
                if (cb->direction == Qt::LeftToRight)
                    tr.setRight(r.left());
                else
                    tr.setLeft(r.right());
            }
        }
        else if (tr.width() <= r.width()) // paint on used part
            tr = r;
        else
        {   // paint centered
            tr = RECT;
            drawItemText(painter, tr.adjusted(-1, -1, -1, -1), Qt::AlignCenter, PAL, isEnabled, cb->text, bg);
            drawItemText(painter, tr.adjusted(1, 1, 1, 1), Qt::AlignCenter, PAL, isEnabled, cb->text, bg);
        }
        drawItemText(painter, tr, Qt::AlignCenter, PAL, isEnabled, cb->text, widget ?
                                                                             widget->foregroundRole() :
                                                                             QPalette::WindowText);
    }
}

void
Style::drawListViewProgress(const QStyleOptionProgressBar *option, QPainter *painter, const QWidget *) const
{   // TODO: widget doesn't set a state - make bug report!
    OPT_ENABLED;
    if (appType == KTorrent)
        isEnabled = true; // ....

    const QStyleOptionProgressBarV2 *pb2 = qstyleoption_cast<const QStyleOptionProgressBarV2*>(option);
    QPalette::ColorRole fg = QPalette::Text, bg = QPalette::Base;
    if (option->state & State_Selected)
        { fg = QPalette::HighlightedText; bg = QPalette::Highlight; }

    bool reverse = option->direction == Qt::RightToLeft;
    if (pb2 && pb2->invertedAppearance)
        reverse = !reverse;
    const bool vertical = (pb2 && pb2->orientation == Qt::Vertical);
    double val = option->progress / double(option->maximum - option->minimum);

    QString text = option->text.isEmpty() ? QString(" %1% ").arg((int)(val*100)) : " " + option->text + " ";
    QRect r = painter->boundingRect(RECT, Qt::AlignLeft | Qt::AlignVCenter, text);

    QPen oldPen = painter->pen();
    painter->setPen(QPen(Colors::mid(COLOR(fg), COLOR(bg)), F(3)));
    if (vertical)
    {
        painter->drawLine(RECT.x(), RECT.top(), RECT.x(), RECT.bottom());
        painter->setPen(QPen(COLOR(fg), F(3)));
        r.moveBottom(RECT.bottom() - val*(RECT.height()-r.height()));
        painter->drawLine(RECT.x(), RECT.bottom()-val*RECT.height(), RECT.x(), RECT.bottom());
    }
    else
    {
        painter->drawLine(RECT.left(), RECT.bottom(), RECT.right(), RECT.bottom());
        painter->setPen(QPen(COLOR(fg), F(3)));
        const int d = val*(RECT.width()-r.width());
        if (reverse)
        {
            r.moveRight(RECT.right() - d);
            painter->drawLine(RECT.right()-val*RECT.width(), RECT.bottom(), RECT.right(), RECT.bottom());
        }
        else
        {
            r.moveLeft(RECT.left() + d);
            painter->drawLine(RECT.left(), RECT.bottom(), RECT.left()+val*RECT.width(), RECT.bottom());
        }
    }
    drawItemText(painter, r, Qt::AlignHCenter|Qt::AlignTop, PAL, isEnabled, text);
    painter->setPen(oldPen);
}

void
Style::drawProgressBar(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    ASSURE_OPTION(pb, ProgressBar);
    OPT_HOVER

    bool listView = !widget && (appType == KGet || appType == KTorrent);
    if (!listView && widget && qobject_cast<const QAbstractItemView*>(widget))
        listView = true;
    if (listView)
    {   // kinda inline progress in itemview (but unfortunately kget doesn't send a widget)
        drawListViewProgress(pb, painter, widget);
        return;
    }

    // groove + contents ======
    if (widget && widget->testAttribute(Qt::WA_OpaquePaintEvent))
        erase(option, painter, widget);
    step = Animator::Progress::step(widget);
    drawProgressBarGroove(pb, painter, widget);
    drawProgressBarContents(pb, painter, widget);
    // label? =========
    if (hover && pb->textVisible)
        drawProgressBarLabel(pb, painter, widget);
    // reset step!
    step = -1;
}


// a nice round embedded chunk
static inline void
drawShape(QPainter *p, int s, int x = 0, int y = 0, bool outline = true)
{
    s -= 2;
    p->setPen(QPen(QColor(0,0,0,50),2));
    p->drawEllipse(x+1,y+2,s,s);
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(QColor(255,255,255, outline ? 30 : 15),1));
    p->drawEllipse(x,y+1,s+2,s);
}

static QPixmap renderPix;

void
Style::drawProgressBarGC(const QStyleOption *option, QPainter *painter, const QWidget *widget, bool content) const
{
    if (appType == GTK && !content)
        return; // looks really crap

    ASSURE_OPTION(pb, ProgressBar);
    const QStyleOptionProgressBarV2 *pb2 = qstyleoption_cast<const QStyleOptionProgressBarV2*>(pb);

    bool reverse = option->direction == Qt::RightToLeft;
    if (pb2 && pb2->invertedAppearance)
        reverse = !reverse;
    const bool vertical = (pb2 && pb2->orientation == Qt::Vertical);
    
    const bool busy = pb->maximum == 0 && pb->minimum == 0;
    int x,y,l,t;
    RECT.getRect(&x,&y,&l,&t);
    
    if (vertical) // swap width & height...
        { int h = x; x = y; y = h; l = RECT.height(); t = RECT.width(); }

    double val = 0.0;
    if (busy && content)
    {   // progress with undefined duration / stepamount
        if (step < 0)
            step = Animator::Progress::step(widget);
        val = - Animator::Progress::speed() * step / l;
    }
    else
        val = pb->progress / double(pb->maximum - pb->minimum);

    // maybe there's nothing to do for us
    if (content)
        { if (val == 0.0) return; }
    else if (val == 1.0)
        return;

    // calculate chunk dimensions - minimal 16px or space for 10 chunks, maximal the progress thickness
    int s = qMin(qMax(l/10, F(16)), qMin(t, F(20)));
    if (!s) return;
    int ss = (3*s)/4;
    int n = l/s;
    if (!n) return;
    if (vertical || reverse)
    {
        x = vertical ? RECT.bottom() : RECT.right();
        x -= ((l - n*s) + (s - ss))/2 + ss;
        s = -s;
    }
    else
        x += (l - n*s + s - ss)/2;
    y += (t-ss)/2;
    --x; --y;

#if 0 // connection line...
    if (!content || val == 1.0)
    {
        int y2 = y + ss/2;
        painter->setPen(Colors::mid(FCOLOR(Window), FCOLOR(WindowText),3,1));
        if (vertical)
            painter->drawLine(y2-1, x, y2-1, x+(n-1)*s);
        else
            painter->drawLine(x, y2+1, x+(n-1)*s, y2+1);
    }
#endif

    // cause most chunks will look the same we render ONE into a buffer and then just dump that multiple times...
    if (renderPix.width() != ss+2)
    {
        renderPix = QPixmap(ss+2, ss+2);
        renderPix.fill(Qt::transparent);
    }

    QPainter p(&renderPix);
    p.setRenderHint(QPainter::Antialiasing);

    // draw a chunk
    int nn = (val < 0) ? 0 : int(n*val);
    if (content)
        p.setBrush(Gradients::brush(CCOLOR(progress._, Fg), ss, Qt::Vertical, GRAD(progress) ));
    else
    {   // this is the "not-yet-done" part - in case we're currently painting it...
        if (busy)
            nn = n;
        else
            { x += nn*s; nn = n - nn; }
        const QColor c = CCOLOR(progress._, Bg);
        p.setBrush(Gradients::brush(c, ss, Qt::Vertical, GRAD(progress) ));
    }
    p.setBrushOrigin(0,1);
    drawShape(&p, ss);
    p.end();

    if (vertical) // x is in fact y!
        for (int i = 0; i < nn; ++i)
            { painter->drawPixmap(y,x, renderPix); x+=s; }
    else // x is as expected... - gee my math teacher was actually right: "always label the axis!"
        for (int i = 0; i < nn; ++i)
            { painter->drawPixmap(x,y, renderPix); x+=s; }

    // cleanup for later
    renderPix.fill(Qt::transparent);

    // if we're painting the actual progress, ONE chunk may be "semifinished" - that's done below
    if (content)
    {
        bool b = (nn < n);
//       x+=2; y+=2; ss-=2;
        if (busy)
        {   // the busy indicator has always a semifinished item, but we need to calculate which first
            b = true;
            val = -val; nn = int(n*val); x += nn*s;
            double o = n*val - nn;
            if (o < .5)
                val += o/n;
            else
                val += (1.0-2*o)/n;
        }
        if (b)
        {
            int q = int((10*n)*val) - 10*nn;
            if (q)
            {
                const QColor c = Colors::mid(CCOLOR(progress._, Bg), CCOLOR(progress._, Fg), 10-q, q);

                if (vertical) // swap again, we abuse 'q' from above
                    { q = x; x = y; y = q; }

                painter->save();
                painter->setRenderHint(QPainter::Antialiasing);
                painter->setBrush(Gradients::brush(c, ss, Qt::Vertical, GRAD(progress) ));
                painter->setBrushOrigin(0, y);
                drawShape(painter, ss, x, y, false);
                painter->restore();
            }
        }
    }
}

void
Style::drawProgressBarLabel(const QStyleOption *option, QPainter *painter, const QWidget*) const
{
    ASSURE_OPTION(progress, ProgressBarV2);
    OPT_HOVER

    if (!(hover && progress->textVisible))
        return;

    painter->save();
    QRect rect = RECT;
    if (progress->orientation == Qt::Vertical)
    {   // vertical progresses have text rotated by 90�� or 270��
        QMatrix m;
        int h = rect.height(); rect.setHeight(rect.width()); rect.setWidth(h);
        if (progress->bottomToTop)
            { m.translate(0.0, RECT.height()); m.rotate(-90); }
        else
            { m.translate(RECT.width(), 0.0); m.rotate(90); }
        painter->setMatrix(m);
    }
    int flags = Qt::AlignCenter | Qt::TextSingleLine;
    QRect tr = painter->boundingRect(rect, flags, progress->text);
    if (!tr.isValid())
        { painter->restore(); return; }
    tr.adjust(-F(6), -F(3), F(6), F(3));
    Tile::setShape(Tile::Full);
    QColor bg = FCOLOR(Window); bg.setAlpha(200);
    masks.rect[true].render(tr, painter, bg);
    Tile::reset();
    painter->setPen(FCOLOR(WindowText));
    painter->drawText(rect, flags, progress->text);
    painter->restore();
}

//    case PE_IndicatorProgressChunk: // Section of a progress bar indicator; see also QProgressBar.
