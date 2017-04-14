#include "graphics_scroll_bar.h"
#include "graphics_scroll_bar_p.h"

#include <limits.h>

#include <QtCore/QEvent>

#include <QtGui/QCursor>
#include <QtGui/QPainter>

#include <QtWidgets/QApplication>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QMenu>
#include <QtWidgets/QGraphicsSceneContextMenuEvent>

namespace {
    QPointF cwiseDiv(const QPointF &l, const QSizeF &r) {
        return QPointF(
            l.x() / r.width(),
            l.y() / r.height()
        );
    }

    QPointF cwiseMul(const QPointF &l, const QSizeF &r) {
        return QPointF(
            l.x() * r.width(),
            l.y() * r.height()
        );
    }

} // anonymous namespace

void GraphicsScrollBarPrivate::setSliderPositionIgnoringAdjustments(qint64 position) {
    bool adjustForPressedHandle = this->adjustForPressedHandle;
    this->adjustForPressedHandle = false;
    q_func()->setSliderPosition(position);
    this->adjustForPressedHandle = adjustForPressedHandle;
}

bool GraphicsScrollBarPrivate::updateHoverControl(const QPointF &pos)
{
    Q_Q(GraphicsScrollBar);
    QRect lastHoverRect = hoverRect;
    QStyle::SubControl lastHoverControl = hoverControl;
    bool doesHover = q->acceptHoverEvents();
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
    hoverControl = q->style()->hitTestComplexControl(QStyle::CC_ScrollBar, &opt, pos.toPoint(), NULL);
    if (hoverControl == QStyle::SC_None)
        hoverRect = QRect();
    else
        hoverRect = q->style()->subControlRect(QStyle::CC_ScrollBar, &opt, hoverControl, NULL);
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
    q->update(q->style()->subControlRect(QStyle::CC_ScrollBar, &opt, tmp, NULL));
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

GraphicsScrollBar::GraphicsScrollBar(Qt::Orientation orientation, QGraphicsItem *parent)
    : base_type(*new GraphicsScrollBarPrivate, parent)
{
    d_func()->orientation = orientation;
    d_func()->init();
}

GraphicsScrollBar::GraphicsScrollBar(GraphicsScrollBarPrivate &dd, Qt::Orientation orientation, QGraphicsItem *parent)
    : base_type(dd, parent)
{
    d_func()->orientation = orientation;
    d_func()->init();
}

GraphicsScrollBar::~GraphicsScrollBar()
{
}

void GraphicsScrollBarPrivate::init()
{
    Q_Q(GraphicsScrollBar);

    initControls(QStyle::CC_ScrollBar, QStyle::SC_ScrollBarGroove, QStyle::SC_ScrollBarSlider);

    adjustForPressedHandle = true;
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
void GraphicsScrollBar::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (!style()->styleHint(QStyle::SH_ScrollBar_ContextMenu, 0, NULL)) {
        base_type::contextMenuEvent(event);
        return;
    }

#ifndef QT_NO_MENU
    bool horiz = d_func()->orientation == Qt::Horizontal;
    QPointer<QMenu> guard = new QMenu(event->widget());
    QMenu *menu = guard.data();

    QAction *actScrollHere = menu->addAction(tr("Scroll Here"));
    menu->addSeparator();
    QAction *actScrollTop =  menu->addAction(horiz ? tr("Left Edge") : tr("Top"));
    QAction *actScrollBottom = menu->addAction(horiz ? tr("Right Edge") : tr("Bottom"));
    menu->addSeparator();
    QAction *actPageUp = menu->addAction(horiz ? tr("Page Left") : tr("Page Up"));
    QAction *actPageDn = menu->addAction(horiz ? tr("Page Right") : tr("Page Down"));
    menu->addSeparator();
    QAction *actScrollUp = menu->addAction(horiz ? tr("Scroll Left") : tr("Scroll Up"));
    QAction *actScrollDn = menu->addAction(horiz ? tr("Scroll Right") : tr("Scroll Down"));
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


QSizeF GraphicsScrollBar::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    if(which != Qt::MinimumSize && which != Qt::PreferredSize)
        return base_type::sizeHint(which, constraint);

    QStyleOptionSlider opt;
    initStyleOption(&opt);

    int scrollBarExtent = style()->pixelMetric(QStyle::PM_ScrollBarExtent, &opt, NULL);
    int scrollBarSliderMin = style()->pixelMetric(QStyle::PM_ScrollBarSliderMin, &opt, NULL);
    QSize size;
    if (opt.orientation == Qt::Horizontal)
        size = QSize(scrollBarExtent * 2 + scrollBarSliderMin, scrollBarExtent);
    else
        size = QSize(scrollBarExtent, scrollBarExtent * 2 + scrollBarSliderMin);

    return style()->sizeFromContents(QStyle::CT_ScrollBar, &opt, size, NULL).expandedTo(QApplication::globalStrut());
}

void GraphicsScrollBar::sliderChange(SliderChange change) {
    Q_D(GraphicsScrollBar);

    base_type::sliderChange(change);

    if(d->adjustForPressedHandle && d->pressedControl == QStyle::SC_ScrollBarSlider && (change == SliderStepsChange || change == SliderValueChange))
        sendPendingMouseMoves(false);
}

bool GraphicsScrollBar::sceneEvent(QEvent *event)
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

    return base_type::sceneEvent(event);
}

void GraphicsScrollBar::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget)
{
    Q_D(GraphicsScrollBar);

    sendPendingMouseMoves(widget);

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
    style()->drawComplexControl(QStyle::CC_ScrollBar, &opt, painter, NULL);
}

void GraphicsScrollBar::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Q_D(GraphicsScrollBar);

    base_type::mousePressEvent(event);

    if (d->repeatActionTimer.isActive())
        d->stopRepeatAction();

    bool midButtonAbsPos = style()->styleHint(QStyle::SH_ScrollBar_MiddleClickAbsolutePosition, 0, NULL);
    QStyleOptionSlider opt;
    initStyleOption(&opt);

    if (d->maximum == d->minimum // no range
        || (event->buttons() & (~event->button())) // another button was clicked before
        || !(event->button() == Qt::LeftButton || (midButtonAbsPos && event->button() == Qt::MidButton)))
        return;

    d->pressedControl = style()->hitTestComplexControl(QStyle::CC_ScrollBar, &opt, event->pos().toPoint(), NULL);
    d->pointerOutsidePressedControl = false;

    QRectF sliderRect = style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, NULL);
    QPointF click = event->pos();
    QPointF pressValue = click - sliderRect.center() + sliderRect.topLeft();
    d->pressValue = valueFromPosition(pressValue);
    if (d->pressedControl == QStyle::SC_ScrollBarSlider) {
        d->relativeClickOffset = cwiseDiv(click - sliderRect.topLeft(), sliderRect.size());
        d->snapBackPosition = d->position;
    }

    if ((d->pressedControl == QStyle::SC_ScrollBarAddPage
          || d->pressedControl == QStyle::SC_ScrollBarSubPage)
        && ((midButtonAbsPos && event->button() == Qt::MidButton)
            || (style()->styleHint(QStyle::SH_ScrollBar_LeftClickAbsolutePosition, &opt, NULL)
                && event->button() == Qt::LeftButton))) {

        d->relativeClickOffset = QPointF(0.5, 0.5);
        setSliderPosition(valueFromPosition(event->pos() - d->relativeClickOffset));
        d->pressedControl = QStyle::SC_ScrollBarSlider;
    }

    d->activateControl(d->pressedControl, 500);
    update(style()->subControlRect(QStyle::CC_ScrollBar, &opt, d->pressedControl, NULL));

    if (d->pressedControl == QStyle::SC_ScrollBarSlider)
        setSliderDown(true);
}

void GraphicsScrollBar::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_D(GraphicsScrollBar);

    base_type::mouseReleaseEvent(event);

    if (!d->pressedControl)
        return;

    if (event->buttons() & (~event->button())) // some other button is still pressed
        return;

    d->stopRepeatAction();
}

void GraphicsScrollBar::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    Q_D(GraphicsScrollBar);

    base_type::mouseMoveEvent(event);

    if (!d->pressedControl)
        return;

    QStyleOptionSlider opt;
    initStyleOption(&opt);

    if (!(event->buttons() & Qt::LeftButton
          ||  ((event->buttons() & Qt::MidButton)
               && style()->styleHint(QStyle::SH_ScrollBar_MiddleClickAbsolutePosition, &opt, NULL))))
        return;

    if (d->pressedControl == QStyle::SC_ScrollBarSlider) {
        QRectF sliderRect = style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, NULL);
        qint64 newPosition = valueFromPosition(event->pos() - cwiseMul(d->relativeClickOffset, sliderRect.size()));
        int m = style()->pixelMetric(QStyle::PM_MaximumDragDistance, &opt, NULL);
        if (m >= 0) {
            QRectF r = rect();
            r.adjust(-m, -m, m, m);
            if (!r.contains(event->pos()))
                newPosition = d->snapBackPosition;
        }
        d->setSliderPositionIgnoringAdjustments(newPosition);
    } else if (!style()->styleHint(QStyle::SH_ScrollBar_ScrollWhenPointerLeavesControl, &opt, NULL)) {
        if (style()->styleHint(QStyle::SH_ScrollBar_RollBetweenButtons, &opt, NULL)
                && d->pressedControl & (QStyle::SC_ScrollBarAddLine | QStyle::SC_ScrollBarSubLine)) {
            QStyle::SubControl newSc = style()->hitTestComplexControl(QStyle::CC_ScrollBar, &opt, event->pos().toPoint(), NULL);
            if (newSc == d->pressedControl && !d->pointerOutsidePressedControl)
                return; /* Nothing to do. */
            if (newSc & (QStyle::SC_ScrollBarAddLine | QStyle::SC_ScrollBarSubLine)) {
                d->pointerOutsidePressedControl = false;
                QRect scRect = style()->subControlRect(QStyle::CC_ScrollBar, &opt, newSc, NULL);
                scRect |= style()->subControlRect(QStyle::CC_ScrollBar, &opt, d->pressedControl, NULL);
                d->pressedControl = newSc;
                d->activateControl(d->pressedControl, 0);
                update(scRect);
                return;
            }
        }

        /* Stop scrolling when the mouse pointer leaves a control, similar to push buttons. */
        QRectF pressedRect = style()->subControlRect(QStyle::CC_ScrollBar, &opt, d->pressedControl, NULL);
        if (pressedRect.contains(event->pos()) == d->pointerOutsidePressedControl) {
            if ((d->pointerOutsidePressedControl = !d->pointerOutsidePressedControl)) {
                d->pointerOutsidePressedControl = true;
                setRepeatAction(SliderNoAction);
                update(pressedRect);
            } else  {
                d->activateControl(d->pressedControl);
            }
        }
    }
}

void GraphicsScrollBar::hideEvent(QHideEvent *)
{
    Q_D(GraphicsScrollBar);
    if (d->pressedControl) {
        d->pressedControl = QStyle::SC_None;
        setRepeatAction(SliderNoAction);
    }
}

