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

#ifdef QT3_SUPPORT
#include <Qt3Support/Q3ScrollView>
#endif
#include <QAbstractScrollArea>
#include <QApplication>
#include <QTimer>
#include "draw.h"
#include "animator/hover.h"
#include "animator/hovercomplex.h"

inline static bool
scrollAreaHovered(const QWidget* slider)
{
//     bool scrollerActive = false;
    if (!slider) return true;
    QWidget *scrollWidget = const_cast<QWidget*>(slider);
    if (!scrollWidget->isEnabled())
        return false;
    while (scrollWidget && !(qobject_cast<QAbstractScrollArea*>(scrollWidget) ||
#ifdef QT3_SUPPORT
                                qobject_cast<Q3ScrollView*>(scrollWidget) ||
#endif
                                Animator::Hover::managesArea(scrollWidget)))
        scrollWidget = const_cast<QWidget*>(scrollWidget->parentWidget());
    bool isActive = true;
    if (scrollWidget)
    {
        if (!scrollWidget->underMouse())
            return false;
//         QAbstractScrollArea* scrollWidget = (QAbstractScrollArea*)daddy;
        QPoint tl = scrollWidget->mapToGlobal(QPoint(0,0));
        QRegion scrollArea(tl.x(), tl.y(), scrollWidget->width(), scrollWidget->height());
        QList<QAbstractScrollArea*> scrollChilds = scrollWidget->findChildren<QAbstractScrollArea*>();
        for (int i = 0; i < scrollChilds.size(); ++i)
        {
            QPoint tl = scrollChilds[i]->mapToGlobal(QPoint(0,0));
            scrollArea -= QRegion(tl.x(), tl.y(), scrollChilds[i]->width(), scrollChilds[i]->height());
        }
#ifdef QT3_SUPPORT
        QList<Q3ScrollView*> scrollChilds2 = scrollWidget->findChildren<Q3ScrollView*>();
        for (int i = 0; i < scrollChilds2.size(); ++i)
        {
            QPoint tl = scrollChilds2[i]->mapToGlobal(QPoint(0,0));
            scrollArea -= QRegion(tl.x(), tl.y(), scrollChilds2[i]->width(), scrollChilds2[i]->height());
        }
#endif
//         scrollerActive = scrollArea.contains(QCursor::pos());
        isActive = scrollArea.contains(QCursor::pos());
    }
    return isActive;
}

#define PAINT_ELEMENT(_E_)\
if (scrollbar->subControls & SC_ScrollBar##_E_)\
{\
    optCopy.rect = scrollbar->rect;\
    optCopy.state = saveFlags;\
    optCopy.rect = subControlRect(CC_ScrollBar, &optCopy, SC_ScrollBar##_E_, widget);\
    if (optCopy.rect.isValid())\
    {\
        if (!(scrollbar->activeSubControls & SC_ScrollBar##_E_))\
            optCopy.state &= ~(State_Sunken | State_MouseOver);\
        if (info && (info->fades[Animator::In] & SC_ScrollBar##_E_ ||\
                    info->fades[Animator::Out] & SC_ScrollBar##_E_))\
            complexStep = info->step(SC_ScrollBar##_E_);\
        else \
            complexStep = 0; \
        drawScrollBar##_E_(&optCopy, cPainter, widget);\
    }\
}//

static bool isComboDropDownSlider, scrollAreaHovered_;
static int complexStep, widgetStep;


static QPixmap *scrollBgCache = 0;
static QTimer cacheCleaner;
const static QWidget *cachedScroller = 0;
static QPainter *cPainter = 0;

void
Style::clearScrollbarCache()
{
    cacheCleaner.stop(); cachedScroller = 0L;
    delete scrollBgCache; scrollBgCache = 0L;
}

enum SA_Flags { WebKit = 1, ComboBox = 2 };

static const QWidget *last_widget = 0;
static int last_flags = 0;

static void updateLastWidget( const QWidget *widget )
{
    if (widget != last_widget)
    {
        last_widget = widget;
        last_flags = 0;
        if (widget->inherits("QWebView"))
            last_flags |= WebKit;
        else if (widget->testAttribute(Qt::WA_OpaquePaintEvent) &&
                                    widget->parentWidget() &&
                    widget->parentWidget()->parentWidget() &&
                    widget->parentWidget()->parentWidget()->inherits("QComboBoxListView"))
            last_flags |= ComboBox;
    }
}

void
Style::drawScrollAreaCorner(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // ouchhh...!
    if (widget)
    {
        updateLastWidget(widget);
        if (last_flags & WebKit)
            erase(option, painter, widget);
    }
}

void
Style::drawScrollBar(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{

    ASSURE_OPTION(scrollbar, Slider);

    cPainter = painter;
    bool useCache = false, needsPaint = true;
    bool isWebKit = false; // ouchhh... needs some specials
    if (widget)
    {
        updateLastWidget(widget);
        if ((isWebKit = last_flags & WebKit))
        {
            const bool isFrame = RECT.height() == widget->height() || RECT.height() == widget->height() - RECT.width() ||
                                 RECT.width() == widget->width() || RECT.width() == widget->width() - RECT.height();
            if (isFrame)
                last_flags &= ~ComboBox;
            else
                last_flags |= ComboBox;
        }
    }

    // we paint the slider bg ourselves, as otherwise a frame repaint would be
    // triggered (for no sense)
    if (!widget) // fallback ===========
        painter->fillRect(RECT, FCOLOR(Window));
    
    else if (isWebKit || widget->testAttribute(Qt::WA_OpaquePaintEvent))
    {   /// fake a transparent bg (real transparency leads to frame22 painting overhead)
        // i.e. we erase the bg with the window background or any autofilled element between

        if ( last_flags & ComboBox )
        {   /// catch combobox dropdowns ==========
            painter->fillRect(RECT, PAL.brush(QPalette::Base));
            isComboDropDownSlider = true;
        }

        else
        {   /// default scrollbar ===============
            isComboDropDownSlider = false;

            if (option->state & State_Sunken)
            {   /// use caching for sliding scrollers to gain speed
                useCache = true;
                if (widget != cachedScroller)
                {   // update cache
                    cachedScroller = widget;
                    delete scrollBgCache; scrollBgCache = 0L;
                }
                if (!scrollBgCache || scrollBgCache->size() != RECT.size())
                {   // we need a new cache pixmap
                    cacheCleaner.disconnect();
                    connect(&cacheCleaner, SIGNAL(timeout()), this, SLOT(clearScrollbarCache()));
                    delete scrollBgCache;
                    scrollBgCache = new QPixmap(RECT.size());
                    if (config.bg.opacity != 0xff)
                        scrollBgCache->fill(Qt::transparent);
                    cPainter = new QPainter(scrollBgCache);
                }
                else
                    needsPaint = false;
            }
            if (needsPaint)
            {
                QPoint offset(0,0);
                if (isWebKit && (option->state & QStyle::State_Horizontal))
                    offset.setY(RECT.height() - widget->height());
                erase(option, cPainter, widget, &offset);
            }
        }
    }
    // =================

    //BEGIN real scrollbar painting                                                                -
   
    // Make a copy here and reset it for each primitive.
    QStyleOptionSlider optCopy = *scrollbar;
    if (isWebKit)
        optCopy.palette = QApplication::palette();
    State saveFlags = optCopy.state;
    if (scrollbar->minimum == scrollbar->maximum)
        saveFlags &= ~State_Enabled; // there'd be nothing to scroll anyway...
        
    /// hover animation =================
    if (scrollbar->activeSubControls & SC_ScrollBarSlider)
        { widgetStep = 0; scrollAreaHovered_ = true; }
    else
    {
        widgetStep = Animator::Hover::step(widget);
        scrollAreaHovered_ = !isWebKit && scrollAreaHovered(widget);
    }
    
    SubControls hoverControls = scrollbar->activeSubControls &
                                (SC_ScrollBarSubLine | SC_ScrollBarAddLine | SC_ScrollBarSlider);
    const Animator::ComplexInfo *info = isWebKit ? 0L : Animator::HoverComplex::info(widget, hoverControls);
    /// ================

    QRect groove;
    if (needsPaint)
    {   // NOTICE the scrollbar bg is cached for sliding scrollers to gain speed
        // the cache also includes the groove
        PAINT_ELEMENT(Groove);
        groove = optCopy.rect;
    }
    else
        groove = subControlRect(CC_ScrollBar, &optCopy, SC_ScrollBarGroove, widget);

    if (cPainter != painter) // unwrap cache painter
        { cPainter->end(); delete cPainter; cPainter = painter; }
   
    /// Background and groove have been painted
    if (useCache) //flush the cache
    {   cacheCleaner.start(1000);
        painter->drawPixmap(RECT.topLeft(), *scrollBgCache);
    }

    if (config.scroll.showButtons)
    {   // nasty useless "click-to-scroll-one-single-line" buttons
        PAINT_ELEMENT(SubLine);
        PAINT_ELEMENT(AddLine);
    }

    const bool grooveIsSunken = config.scroll.groove > Groove::Groove;

    if ((saveFlags & State_Enabled) && (scrollbar->subControls & SC_ScrollBarSlider))
    {
        optCopy.rect = scrollbar->rect;
        optCopy.state = saveFlags;
        optCopy.rect = subControlRect(CC_ScrollBar, &optCopy, SC_ScrollBarSlider, widget);
        if (grooveIsSunken)
            optCopy.rect.adjust(-F(1),-F(1),F(1),0);

        if (optCopy.rect.isValid())
        {
            if (!(scrollbar->activeSubControls & SC_ScrollBarSlider))
                optCopy.state &= ~(State_Sunken | State_MouseOver);

            if (scrollbar->state & State_HasFocus)
                optCopy.state |= (State_Sunken | State_MouseOver);

            if (info && (   (info->fades[Animator::In] & SC_ScrollBarSlider) ||
                            (info->fades[Animator::Out] & SC_ScrollBarSlider)   ))
                complexStep = info->step(SC_ScrollBarSlider);
            else
                complexStep = 0;

            drawScrollBarSlider(&optCopy, cPainter, widget);
        }
    }
   
//     if (!isComboDropDownSlider && grooveIsSunken)
//         shadows.sunken[round_][isEnabled].render(groove, painter);

    isComboDropDownSlider = scrollAreaHovered_ = false;
    widgetStep = complexStep = 0;
}
#undef PAINT_ELEMENT

void
Style::drawScrollBarButton(const QStyleOption *option, QPainter *painter, const QWidget*, bool up) const
{
    ASSURE_OPTION(opt, Slider);

    if (isComboDropDownSlider)
    {   // gets a classic triangular look and is allways shown
        OPT_HOVER

        painter->save();
        painter->setPen(hover ? FCOLOR(Text) : Colors::mid(FCOLOR(Base), FCOLOR(Text)));
        const int dx = RECT.width()/4, dy = RECT.height()/4;
        QRect rect = RECT.adjusted(dx, dy, -dx, -dy);
        if (option->state & QStyle::State_Horizontal)
            drawSolidArrow(up ? Navi::W : Navi::E, rect, painter);
        else
            drawSolidArrow(up ? Navi::N : Navi::S, rect, painter);
        painter->restore();
        return;
    }

    if (!config.scroll.showButtons)
        return;

    OPT_SUNKEN OPT_ENABLED OPT_HOVER

    QRect r = RECT.adjusted(F(2),F(2),-F(2),-F(2));
    bool alive = isEnabled && // visually inactivate if an extreme position is reached
                ((up && opt->sliderValue > opt->minimum) || (!up && opt->sliderValue < opt->maximum));
    hover = hover && alive;
    const int step = (hover && !complexStep) ? 6 : complexStep;
    
    const QColor c = alive ? Colors::mid(CCOLOR(btn.std, Bg), CCOLOR(btn.active, Bg), 6-step, step) : FCOLOR(Window);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (alive)
        painter->setPen(CCOLOR(btn.std, Bg).dark(120));
    else
        painter->setPen(Qt::NoPen);
    const QPixmap &fill = Gradients::pix(c, r.height(), Qt::Vertical,
                                         (sunken || !alive) ? Gradients::Sunken : Gradients::Button);
    painter->setBrush(fill);
    painter->setBrushOrigin(r.topLeft());
    painter->drawEllipse(r);
    painter->restore();
}

void
Style::drawScrollBarGroove(const QStyleOption *option, QPainter *painter, const QWidget *) const
{
    OPT_ENABLED;
    const bool horizontal = option->state & QStyle::State_Horizontal ||
                            RECT.width() > RECT.height(); // at least opera didn't propagate this

    if (isComboDropDownSlider)
    {   // get's a special solid groove
        QRect r;
        if (horizontal)
        {
            const int d = 2*RECT.height()/5;
            r = RECT.adjusted(F(2), d, -F(2), -d);
        }
        else
        {
            const int d = 2*RECT.width()/5;
            r = RECT.adjusted(d, F(2), -d, -F(2));
        }
        painter->fillRect(r, Colors::mid(FCOLOR(Base), FCOLOR(Text), 10, 1));
        return;
    }

    const Groove::Mode gType = config.scroll.groove;
    const bool round = config.scroll.sliderWidth + (gType > Groove::Groove) > 13;
    
    if ( appType == Opera && painter->device() )
    {   // opera fakes raised webpages - we help a little ;-)
        QRect r( 0, 0, painter->device()->width(), painter->device()->height() );
        if ( horizontal )
        {
            const QPixmap &shadow = shadows.raised[false][true][false].tile(Tile::BtmMid);
            r.setHeight( shadow.height() );
            painter->drawTiledPixmap( r, shadow );
        }
        else
        {
            const QPixmap &shadow = shadows.raised[false][true][false].tile(Tile::MidRight);
            r.setWidth( shadow.width() );
            painter->drawTiledPixmap( r, shadow );
        }
        return; // no real "groove"...
    }

    QColor bg = config.scroll.invertBg ?
                Colors::mid(FCOLOR(WindowText), FCOLOR(Window), 2 + gType*gType*gType, 1):
                Colors::mid(FCOLOR(Window), FCOLOR(WindowText), 2 + gType*gType*gType, 1);
    
    if (gType == Groove::Line)
    {
        SAVE_PEN;
        painter->setPen(QPen(bg, F(1)));
        QPoint c = RECT.center();
        if (horizontal)
            painter->drawLine(RECT.left() + F(4), c.y(), RECT.right() - F(4), c.y());
        else
            painter->drawLine(c.x(), RECT.top() + F(4), c.x(), RECT.bottom() - F(4));
        RESTORE_PEN;
    }
    else if (gType == Groove::Groove)
        masks.rect[round].render(RECT, painter, Gradients::Sunken, horizontal ? Qt::Vertical : Qt::Horizontal, bg);
    else
    {
        if (gType == Groove::SunkenGroove)
            masks.rect[round].render(RECT.adjusted(0,0,0,-F(2)), painter, Gradients::Sunken, horizontal ? Qt::Vertical : Qt::Horizontal, bg);
        shadows.sunken[round][isEnabled].render(RECT, painter);
    }
    return;
}

void
Style::drawScrollBarSlider(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    OPT_SUNKEN OPT_ENABLED OPT_HOVER
    const bool horizontal = option->state & QStyle::State_Horizontal ||
                             // at least opera doesn't propagate this
                             RECT.width() > RECT.height();

    if (isComboDropDownSlider)
    {   //gets a special slimmer and simpler look
        QRect r = RECT;
        if (horizontal)
        {
            const int d = RECT.height()/3;
            r.adjust(F(2), d, -F(2), -d);
        }
        else
        {
            const int d = RECT.width()/3;
            r.adjust(d, F(2), -d, -F(2));
        }
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setRenderHint(QPainter::Antialiasing);
        if (sunken || (hover && !complexStep))
            complexStep = 6;

        painter->setBrush(Colors::mid(FCOLOR(Base), FCOLOR(Text), 6-complexStep, complexStep+1));
        painter->drawRoundedRect(r, F(4), F(4));
        painter->restore();
        return;
    }

    if (!isEnabled)
    {   // fallback, only if the slider primitive is painted directly
        if (config.scroll.groove != Groove::Sunken)
            drawScrollBarGroove(option, painter, widget);
        return;
    }

    // --> we need to paint a slider

    // COLOR: the hover indicator (inside area)
    const bool backLightHover = config.btn.backLightHover && config.scroll.sliderWidth > 9;
#define SCROLL_COLOR(_X_) \
(widgetStep ? Colors::mid(  bgC, fgC, (backLightHover ? (Gradients::isReflective(GRAD(scroll)) ? 48 : 72) : 6) - _X_, _X_) : bgC)

    if (scrollAreaHovered_ && !widgetStep)
        widgetStep = 6;

    QColor c, bgC = CCOLOR(scroll._, Bg), fgC = CCOLOR(scroll._, Fg);
    if ( !backLightHover && widget && widget->isActiveWindow() )
    {
        if ( complexStep )
            { if (hover || !scrollAreaHovered_) complexStep = 6; }
        else if (!hover)
            { hover = !widgetStep; bgC = CCOLOR(scroll._, Fg); fgC = CCOLOR(scroll._, Bg); }
    }

    if (sunken)
        c = SCROLL_COLOR(6);
    else if (complexStep)
    {
        c = Colors::mid(bgC, SCROLL_COLOR(widgetStep));
        c = Colors::mid(c, SCROLL_COLOR(complexStep), 6-complexStep, complexStep);
    }
    else if (hover)
        { complexStep = 6; c = SCROLL_COLOR(6); }
    else if (widgetStep)
        c = Colors::mid(bgC, SCROLL_COLOR(widgetStep));
    else
        c = bgC;
    c.setAlpha(255); // bg could be transparent, i don't want scrollers translucent, though.
#undef SCROLL_COLOR
   
    QRect r = RECT;
    const int f1 = F(1), f2 = F(2);
    const bool round = config.scroll.sliderWidth + (config.scroll.groove > Groove::Groove) > 13;
    const bool grooveIsSunken = config.scroll.groove >= Groove::Sunken;

    // draw shadow
    // clip away innper part of shadow - hey why paint invisible alpha stuff =D   --------
    bool hadClip = painter->hasClipping();
    QRegion oldClip;
    if (hadClip)
        oldClip = painter->clipRegion();
    painter->setClipping(true);
    if (horizontal)
        painter->setClipRegion(QRegion(RECT) - r.adjusted(F(9), F(3), -F(9), -F(3)));
    else
        painter->setClipRegion(QRegion(RECT) - r.adjusted(F(3), F(9), -F(3), -F(9)));
    // --------------
    if (sunken && !grooveIsSunken)
    {
        r.adjust(f1, f1, -f1, -f1);
        shadows.raised[round][true][true].render(r, painter);
        r.adjust(f1, f1, -f1, -f2 );
    }
    else
    {
        if (!sunken && backLightHover && complexStep)
        {
            QColor blh = Colors::mid(c, CCOLOR(scroll._, Fg), 6-complexStep, complexStep);
            lights.rect[round].render(r, painter, blh); // backlight
        }
        shadows.raised[round][true][true].render(r, painter);
        r.adjust(f2, f2, -f2, horizontal && grooveIsSunken ? -f2 : -F(3) );
    }
    // restore clip---------------
    if (hadClip)
        // sic! clippping e.g. in webkit seems to be broken? at least querky with size and pos twisted...
        painter->setClipRegion(RECT);
    painter->setClipping(hadClip);

    // the always shown base
    Qt::Orientation o = Qt::Horizontal;
    QPoint offset;
    int size = r.width();
    if (horizontal || config.showOff)
    {
        o = Qt::Vertical; size = r.height();
        if (!config.showOff)
            offset.setX(-r.left()/2);
    }
    else
        offset.setY(-r.top()/2);

    const bool fullHover = config.scroll.fullHover || config.scroll.sliderWidth < 10;
    QColor bc = fullHover ? c : CCOLOR(scroll._, Bg);
    bc.setAlpha(255); // CCOLOR(scroll._, Bg) pot. reintroduces translucency...
    masks.rect[round].render(r, painter, GRAD(scroll), o, bc, size, offset);

    // reflexive outline
    if ( GRAD(scroll) == Gradients::Shiny || (!sunken && Gradients::isReflective(GRAD(btn))) )
        masks.rect[round].outline(r.adjusted(f1,f1,-f1,-f1), painter, bc.lighter(120), f1);

    // the hover indicator (in case...)
    if (fullHover || !(hover || complexStep || widgetStep))
        return;

    int dw = r.width(), dh = r.height();
    if (horizontal)
        { dw /= 8; dh /= 4; }
    else
        { dw /= 4; dh /= 8; }
    r.adjust(dw, dh, -dw, -dh);
    offset += QPoint(dw, dh);
    masks.rect[false].render(r, painter, GRAD(scroll), o, c, size, offset);
}

//    case CE_ScrollBarFirst: // Scroll bar first line indicator (i.e., home).
//    case CE_ScrollBarLast: // Scroll bar last line indicator (i.e., end).
