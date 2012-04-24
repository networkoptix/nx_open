#include "graphics_scroll_bar.h"
#include "graphics_scroll_bar_p.h"

#include <QtCore/QWeakPointer>

#include <QtGui/QMenu>
#include <QtGui/QGraphicsSceneContextMenuEvent>

#include "qapplication.h"
#include "qcursor.h"
#include "qevent.h"
#include "qpainter.h"
#include "qscrollbar.h"
#include "qstyle.h"
#include "qstyleoption.h"
#include "qmenu.h"
#include <QtCore/qelapsedtimer.h>

#ifndef QT_NO_ACCESSIBILITY
#include "qaccessible.h"
#endif
#include <limits.h>

bool GraphicsScrollBarPrivate::updateHoverControl(const QPointF &pos)
{
    Q_Q(GraphicsScrollBar);
    QRect lastHoverRect = hoverRect;
    QStyle::SubControl lastHoverControl = hoverControl;
    bool doesHover = q->acceptsHoverEvents();
    if (lastHoverControl != newHoverControl(pos) && doesHover) {
        q->update(lastHoverRect);
        q->update(hoverRect);
        return true;
    }
    return !doesHover;
}

QStyle::SubControl GraphicsScrollBarPrivate::newHoverControl(const QPointF &pos)
{
    Q_Q(GraphicsScrollBar);
    QStyleOptionSlider opt;
    q->initStyleOption(&opt);
    opt.subControls = QStyle::SC_All;
    hoverControl = q->style()->hitTestComplexControl(QStyle::CC_ScrollBar, &opt, pos.toPoint(), q);
    if (hoverControl == QStyle::SC_None)
        hoverRect = QRect();
    else
        hoverRect = q->style()->subControlRect(QStyle::CC_ScrollBar, &opt, hoverControl, q);
    return hoverControl;
}

void GraphicsScrollBarPrivate::activateControl(uint control, int threshold)
{
    AbstractGraphicsSlider::SliderAction action = AbstractGraphicsSlider::SliderNoAction;
    switch (control) {
    case QStyle::SC_ScrollBarAddPage:
        action = AbstractGraphicsSlider::SliderPageStepAdd;
        break;
    case QStyle::SC_ScrollBarSubPage:
        action = AbstractGraphicsSlider::SliderPageStepSub;
        break;
    case QStyle::SC_ScrollBarAddLine:
        action = AbstractGraphicsSlider::SliderSingleStepAdd;
        break;
    case QStyle::SC_ScrollBarSubLine:
        action = AbstractGraphicsSlider::SliderSingleStepSub;
        break;
    case QStyle::SC_ScrollBarFirst:
        action = AbstractGraphicsSlider::SliderToMinimum;
        break;
    case QStyle::SC_ScrollBarLast:
        action = AbstractGraphicsSlider::SliderToMaximum;
        break;
    default:
        break;
    }

    if (action) {
        q_func()->setRepeatAction(action, threshold);
        q_func()->triggerAction(action);
    }
}

void GraphicsScrollBarPrivate::stopRepeatAction()
{
    Q_Q(GraphicsScrollBar);
    QStyle::SubControl tmp = pressedControl;
    q->setRepeatAction(AbstractGraphicsSlider::SliderNoAction);
    pressedControl = QStyle::SC_None;

    if (tmp == QStyle::SC_ScrollBarSlider)
        q->setSliderDown(false);

    QStyleOptionSlider opt;
    q->initStyleOption(&opt);
    q->update(q->style()->subControlRect(QStyle::CC_ScrollBar, &opt, tmp, q));
}

void GraphicsScrollBar::initStyleOption(QStyleOption *option) const
{
    base_type::initStyleOption(option);

    if (QStyleOptionSlider *sliderOption = qstyleoption_cast<QStyleOptionSlider *>(option)) {
        Q_D(const GraphicsScrollBar);
        sliderOption->upsideDown = d->invertedAppearance;
        if (d->orientation == Qt::Horizontal)
            sliderOption->state |= QStyle::State_Horizontal;
    }
}

GraphicsScrollBar::GraphicsScrollBar(QGraphicsItem *parent)
    : base_type(*new GraphicsScrollBarPrivate, parent)
{
    d_func()->orientation = Qt::Vertical;
    d_func()->init();
}

/*!
    Constructs a scroll bar with the given \a orientation.

    The \a parent argument is passed to the QWidget constructor.

    The \l {QAbstractSlider::minimum} {minimum} defaults to 0, the
    \l {QAbstractSlider::maximum} {maximum} to 99, with a
    \l {QAbstractSlider::singleStep} {singleStep} size of 1 and a
    \l {QAbstractSlider::pageStep} {pageStep} size of 10, and an
    initial \l {QAbstractSlider::value} {value} of 0.
*/
GraphicsScrollBar::GraphicsScrollBar(Qt::Orientation orientation, QGraphicsItem *parent)
    : base_type(*new GraphicsScrollBarPrivate, parent)
{
    d_func()->orientation = orientation;
    d_func()->init();
}

/*!
    Destroys the scroll bar.
*/
GraphicsScrollBar::~GraphicsScrollBar()
{
}

void GraphicsScrollBarPrivate::init()
{
    Q_Q(GraphicsScrollBar);
    invertedControls = true;
    pressedControl = hoverControl = QStyle::SC_None;
    pointerOutsidePressedControl = false;
    q->setFocusPolicy(Qt::NoFocus);
    QSizePolicy sp(QSizePolicy::Minimum, QSizePolicy::Fixed, QSizePolicy::Slider);
    if (orientation == Qt::Vertical)
        sp.transpose();
    q->setSizePolicy(sp);
}

#ifndef QT_NO_CONTEXTMENU
/*! \reimp */
void GraphicsScrollBar::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (!style()->styleHint(QStyle::SH_ScrollBar_ContextMenu, 0, this)) {
        base_type::contextMenuEvent(event);
        return;
    }

#ifndef QT_NO_MENU
    bool horiz = d_func()->orientation == Qt::Horizontal;
    QWeakPointer<QMenu> guard = new QMenu(event->widget());
    QMenu *menu = guard.data();

    QAction *actScrollHere = menu->addAction(tr("Scroll here"));
    menu->addSeparator();
    QAction *actScrollTop =  menu->addAction(horiz ? tr("Left edge") : tr("Top"));
    QAction *actScrollBottom = menu->addAction(horiz ? tr("Right edge") : tr("Bottom"));
    menu->addSeparator();
    QAction *actPageUp = menu->addAction(horiz ? tr("Page left") : tr("Page up"));
    QAction *actPageDn = menu->addAction(horiz ? tr("Page right") : tr("Page down"));
    menu->addSeparator();
    QAction *actScrollUp = menu->addAction(horiz ? tr("Scroll left") : tr("Scroll up"));
    QAction *actScrollDn = menu->addAction(horiz ? tr("Scroll right") : tr("Scroll down"));
    QAction *actionSelected = menu->exec(event->screenPos());
    if(guard)
        delete menu;

    if (actionSelected == 0) {
        /* do nothing */ ;
    } else if (actionSelected == actScrollHere) {
        setValue(valueFromPosition(event->pos()));
    } else if (actionSelected == actScrollTop) {
        triggerAction(AbstractGraphicsSlider::SliderToMinimum);
    } else if (actionSelected == actScrollBottom) {
        triggerAction(AbstractGraphicsSlider::SliderToMaximum);
    } else if (actionSelected == actPageUp) {
        triggerAction(AbstractGraphicsSlider::SliderPageStepSub);
    } else if (actionSelected == actPageDn) {
        triggerAction(AbstractGraphicsSlider::SliderPageStepAdd);
    } else if (actionSelected == actScrollUp) {
        triggerAction(AbstractGraphicsSlider::SliderSingleStepSub);
    } else if (actionSelected == actScrollDn) {
        triggerAction(AbstractGraphicsSlider::SliderSingleStepAdd);
    }
#endif // QT_NO_MENU
}
#endif // QT_NO_CONTEXTMENU


/*! \reimp */
QSizeF GraphicsScrollBar::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    if(which != Qt::MinimumSize)
        return base_type::sizeHint(which, constraint);

    QStyleOptionSlider opt;
    initStyleOption(&opt);

    int scrollBarExtent = style()->pixelMetric(QStyle::PM_ScrollBarExtent, &opt, this);
    int scrollBarSliderMin = style()->pixelMetric(QStyle::PM_ScrollBarSliderMin, &opt, this);
    QSize size;
    if (opt.orientation == Qt::Horizontal)
        size = QSize(scrollBarExtent * 2 + scrollBarSliderMin, scrollBarExtent);
    else
        size = QSize(scrollBarExtent, scrollBarExtent * 2 + scrollBarSliderMin);

    return style()->sizeFromContents(QStyle::CT_ScrollBar, &opt, size, this).expandedTo(QApplication::globalStrut());
 }

/*!\reimp */
void GraphicsScrollBar::sliderChange(SliderChange change)
{
    base_type::sliderChange(change);
}

/*!
    \reimp
*/
bool GraphicsScrollBar::event(QEvent *event)
{
    switch(event->type()) {
    case QEvent::GraphicsSceneHoverEnter:
    case QEvent::GraphicsSceneHoverLeave:
    case QEvent::GraphicsSceneHoverMove:
    if (const QGraphicsSceneHoverEvent *he = static_cast<const QGraphicsSceneHoverEvent *>(event))
        d_func()->updateHoverControl(he->pos());
        break;
#ifndef QT_NO_WHEELEVENT
    case QEvent::GraphicsSceneWheel: {
        event->ignore();
        // override wheel event without adding virtual function override
        QGraphicsSceneWheelEvent *ev = static_cast<QGraphicsSceneWheelEvent *>(event);
        int delta = ev->delta();
        // scrollbar is a special case - in vertical mode it reaches minimum
        // value in the upper position, however QSlider's minimum value is on
        // the bottom. So we need to invert a value, but since the scrollbar is
        // inverted by default, we need to inverse the delta value for the
        // horizontal orientation.
        if (ev->orientation() == Qt::Horizontal)
            delta = -delta;
        Q_D(GraphicsScrollBar);
        if (d->scrollByDelta(ev->orientation(), ev->modifiers(), delta))
            event->accept();
        return true;
    }
#endif
    default:
        break;
    }
    return base_type::event(event);
}

/*!
    \reimp
*/
void GraphicsScrollBar::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) 
{
    Q_D(GraphicsScrollBar);
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    opt.subControls = QStyle::SC_All;
    if (d->pressedControl) {
        opt.activeSubControls = (QStyle::SubControl) d->pressedControl;
        if (!d->pointerOutsidePressedControl)
            opt.state |= QStyle::State_Sunken;
    } else {
        opt.activeSubControls = (QStyle::SubControl) d->hoverControl;
    }
    style()->drawComplexControl(QStyle::CC_ScrollBar, &opt, painter, this);
}

/*!
    \reimp
*/
void GraphicsScrollBar::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    Q_D(GraphicsScrollBar);

    if (d->repeatActionTimer.isActive())
        d->stopRepeatAction();

    bool midButtonAbsPos = style()->styleHint(QStyle::SH_ScrollBar_MiddleClickAbsolutePosition, 0, this);
    QStyleOptionSlider opt;
    initStyleOption(&opt);

    if (d->maximum == d->minimum // no range
        || (e->buttons() & (~e->button())) // another button was clicked before
        || !(e->button() == Qt::LeftButton || (midButtonAbsPos && e->button() == Qt::MidButton)))
        return;

    d->pressedControl = style()->hitTestComplexControl(QStyle::CC_ScrollBar, &opt, e->pos(), this);
    d->pointerOutsidePressedControl = false;

    QRectF sr = style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, this);
    QPointF click = e->pos();
    QPointF pressValue = click - sr.center() + sr.topLeft();
    d->pressValue = valueFromPosition(pressValue);
    if (d->pressedControl == QStyle::SC_ScrollBarSlider) {
        d->clickOffset = click - sr.topLeft();;
        d->snapBackPosition = d->position;
    }

    if ((d->pressedControl == QStyle::SC_ScrollBarAddPage
          || d->pressedControl == QStyle::SC_ScrollBarSubPage)
        && ((midButtonAbsPos && e->button() == Qt::MidButton)
            || (style()->styleHint(QStyle::SH_ScrollBar_LeftClickAbsolutePosition, &opt, this)
                && e->button() == Qt::LeftButton))) {

        d->clickOffset = QPointF(sr.width(), sr.height()) / 2;
        setSliderPosition(valueFromPosition(e->pos() - d->clickOffset));
        d->pressedControl = QStyle::SC_ScrollBarSlider;
    }

    d->activateControl(d->pressedControl, 500);
    update(style()->subControlRect(QStyle::CC_ScrollBar, &opt, d->pressedControl, this));

    if (d->pressedControl == QStyle::SC_ScrollBarSlider)
        setSliderDown(true);
}


/*!
    \reimp
*/
void GraphicsScrollBar::mouseReleaseEvent(QGraphicsSceneMouseEvent *e)
{
    Q_D(GraphicsScrollBar);
    if (!d->pressedControl)
        return;

    if (e->buttons() & (~e->button())) // some other button is still pressed
        return;

    d->stopRepeatAction();
}


/*!
    \reimp
*/
void GraphicsScrollBar::mouseMoveEvent(QGraphicsSceneMouseEvent *e)
{
    Q_D(GraphicsScrollBar);
    if (!d->pressedControl)
        return;

    QStyleOptionSlider opt;
    initStyleOption(&opt);
    if (!(e->buttons() & Qt::LeftButton
          ||  ((e->buttons() & Qt::MidButton)
               && style()->styleHint(QStyle::SH_ScrollBar_MiddleClickAbsolutePosition, &opt, this))))
        return;

    if (d->pressedControl == QStyle::SC_ScrollBarSlider) {
        QPointF click = e->pos();
        qint64 newPosition = valueFromPosition(click - d->clickOffset);
        int m = style()->pixelMetric(QStyle::PM_MaximumDragDistance, &opt, this);
        if (m >= 0) {
            QRectF r = rect();
            r.adjust(-m, -m, m, m);
            if (!r.contains(e->pos()))
                newPosition = d->snapBackPosition;
        }
        setSliderPosition(newPosition);
    } else if (!style()->styleHint(QStyle::SH_ScrollBar_ScrollWhenPointerLeavesControl, &opt, this)) {

        if (style()->styleHint(QStyle::SH_ScrollBar_RollBetweenButtons, &opt, this)
                && d->pressedControl & (QStyle::SC_ScrollBarAddLine | QStyle::SC_ScrollBarSubLine)) {
            QStyle::SubControl newSc = style()->hitTestComplexControl(QStyle::CC_ScrollBar, &opt, e->pos(), this);
            if (newSc == d->pressedControl && !d->pointerOutsidePressedControl)
                return; // nothing to do
            if (newSc & (QStyle::SC_ScrollBarAddLine | QStyle::SC_ScrollBarSubLine)) {
                d->pointerOutsidePressedControl = false;
                QRect scRect = style()->subControlRect(QStyle::CC_ScrollBar, &opt, newSc, this);
                scRect |= style()->subControlRect(QStyle::CC_ScrollBar, &opt, d->pressedControl, this);
                d->pressedControl = newSc;
                d->activateControl(d->pressedControl, 0);
                update(scRect);
                return;
            }
        }

        // stop scrolling when the mouse pointer leaves a control
        // similar to push buttons
        QRectF pr = style()->subControlRect(QStyle::CC_ScrollBar, &opt, d->pressedControl, this);
        if (pr.contains(e->pos()) == d->pointerOutsidePressedControl) {
            if ((d->pointerOutsidePressedControl = !d->pointerOutsidePressedControl)) {
                d->pointerOutsidePressedControl = true;
                setRepeatAction(SliderNoAction);
                update(pr);
            } else  {
                d->activateControl(d->pressedControl);
            }
        }
    }
}

QPointF GraphicsScrollBar::positionFromValue(qint64 logicalValue) const {
    Q_D(const GraphicsScrollBar);

    QStyleOptionSlider opt;
    initStyleOption(&opt);

    QRectF gr = style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarGroove, this);
    QRectF sr = style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, this);
    qreal sliderMin, sliderMax, sliderLength;

    if (d->orientation == Qt::Horizontal) {
        sliderLength = sr.width();
        sliderMin = gr.x();
        sliderMax = gr.right() - sliderLength + 1;
        if (layoutDirection() == Qt::RightToLeft)
            opt.upsideDown = !opt.upsideDown;
    } else {
        sliderLength = sr.height();
        sliderMin = gr.y();
        sliderMax = gr.bottom() - sliderLength + 1;
    }

    qreal result = sliderMin + GraphicsStyle::sliderPositionFromValue(d->minimum, d->maximum, logicalValue, sliderMax - sliderMin, opt.upsideDown);
    if(d->orientation == Qt::Horizontal) {
        return QPointF(result, 0.0);
    } else {
        return QPointF(0.0, result);
    }
}

qint64 GraphicsScrollBar::valueFromPosition(const QPointF &position) const {
    Q_D(const GraphicsScrollBar);

    QStyleOptionSlider opt;
    initStyleOption(&opt);

    QRectF gr = style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarGroove, this);
    QRectF sr = style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, this);
    qreal sliderMin, sliderMax, sliderLength, pixelPos;

    if (d->orientation == Qt::Horizontal) {
        sliderLength = sr.width();
        sliderMin = gr.x();
        sliderMax = gr.right() - sliderLength + 1;
        if (layoutDirection() == Qt::RightToLeft)
            opt.upsideDown = !opt.upsideDown;
        pixelPos = position.x();
    } else {
        sliderLength = sr.height();
        sliderMin = gr.y();
        sliderMax = gr.bottom() - sliderLength + 1;
        pixelPos = position.y();
    }

    return GraphicsStyle::sliderValueFromPosition(d->minimum, d->maximum, pixelPos - sliderMin, sliderMax - sliderMin, opt.upsideDown);
}

/*! \reimp
*/
void GraphicsScrollBar::hideEvent(QHideEvent *)
{
    Q_D(GraphicsScrollBar);
    if (d->pressedControl) {
        d->pressedControl = QStyle::SC_None;
        setRepeatAction(SliderNoAction);
    }
}

