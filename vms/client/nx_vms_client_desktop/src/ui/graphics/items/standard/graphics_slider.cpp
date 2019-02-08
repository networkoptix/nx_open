#include "graphics_slider.h"
#include "graphics_slider_p.h"

#ifndef QT_NO_ACCESSIBILITY
#include <QtGui/QAccessible>
#endif
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsSceneEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>

void GraphicsSliderPrivate::init()
{
    Q_Q(GraphicsSlider);

    initControls(QStyle::CC_Slider, QStyle::SC_SliderGroove, QStyle::SC_SliderHandle);

    pressedControl = QStyle::SC_None;
    tickPosition = GraphicsSlider::NoTicks;
    tickInterval = 0;
    clickOffset = QPoint(0, 0);
    hoverControl = QStyle::SC_None;

    q->setFocusPolicy(Qt::FocusPolicy(q->style()->styleHint(QStyle::SH_Button_FocusPolicy, NULL, NULL)));
    QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Fixed, QSizePolicy::Slider);
    if (orientation == Qt::Vertical)
        sp.transpose();
    q->setSizePolicy(sp);
}

void GraphicsSliderPrivate::updateHoverControl(const QPoint &pos)
{
    Q_Q(GraphicsSlider);

    const QRect lastHoverRect = hoverRect;
    const QStyle::SubControl lastHoverControl = hoverControl;
    if (lastHoverControl != newHoverControl(pos)) {
        q->update(lastHoverRect);
        q->update(hoverRect);
    }
}

QStyle::SubControl GraphicsSliderPrivate::newHoverControl(const QPoint &pos)
{
    Q_Q(GraphicsSlider);

    QStyleOptionSlider opt;
    q->initStyleOption(&opt);
    opt.subControls = QStyle::SC_All;
    hoverControl = q->style()->hitTestComplexControl(QStyle::CC_Slider, &opt, pos, NULL);
    if (hoverControl != QStyle::SC_None)
        hoverRect = q->style()->subControlRect(QStyle::CC_Slider, &opt, hoverControl, NULL);
    else
        hoverRect = QRect();

    return hoverControl;
}

GraphicsSlider::GraphicsSlider(QGraphicsItem *parent)
    : base_type(*new GraphicsSliderPrivate, parent)
{
    Q_D(GraphicsSlider);
    d->orientation = Qt::Horizontal;
    d->init();
}

GraphicsSlider::GraphicsSlider(Qt::Orientation orientation, QGraphicsItem *parent)
    : base_type(*new GraphicsSliderPrivate, parent)
{
    Q_D(GraphicsSlider);
    d->orientation = orientation;
    d->init();
}

GraphicsSlider::GraphicsSlider(GraphicsSliderPrivate &dd, QGraphicsItem *parent)
    : base_type(dd, parent)
{
}

GraphicsSlider::~GraphicsSlider()
{
}

void GraphicsSlider::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget)
{
    Q_D(GraphicsSlider);

    /* Synchronize current position with the actual mouse position before painting. */
    sendPendingMouseMoves(widget);

    QStyleOptionSlider opt;
    initStyleOption(&opt);
    opt.subControls = QStyle::SC_SliderGroove | QStyle::SC_SliderHandle;
    if (d->tickPosition != NoTicks)
        opt.subControls |= QStyle::SC_SliderTickmarks;
    if (d->pressedControl) {
        opt.activeSubControls = d->pressedControl;
        opt.state |= QStyle::State_Sunken;
    } else {
        opt.activeSubControls = d->hoverControl;
    }

    style()->drawComplexControl(QStyle::CC_Slider, &opt, painter, NULL);
}

bool GraphicsSlider::event(QEvent *event)
{
    Q_D(GraphicsSlider);

    switch(event->type()) {
    case QEvent::GraphicsSceneHoverEnter:
    case QEvent::GraphicsSceneHoverMove:
    case QEvent::GraphicsSceneHoverLeave: {
        QGraphicsSceneHoverEvent *hoverEvent = static_cast<QGraphicsSceneHoverEvent *>(event);
        d->updateHoverControl(hoverEvent->pos().toPoint());
        break;
    }
    default:
        break;
    }

    return base_type::event(event);
}

void GraphicsSlider::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Q_D(GraphicsSlider);

    base_type::mousePressEvent(event);

    if (d->maximum == d->minimum || (event->buttons() ^ event->button())) {
        event->ignore();
        return;
    }

#ifdef QT_KEYPAD_NAVIGATION
    if (QApplication::keypadNavigationEnabled())
        setEditFocus(true);
#endif

    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const QRect sliderRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, NULL);
    // to take half of the slider off for the setSliderPosition call we use the center - topLeft
    const QPoint center = sliderRect.center() - sliderRect.topLeft();
    const qint64 pressValue = valueFromPosition(event->pos().toPoint() - center);

    event->accept();
    if ((event->button() & style()->styleHint(QStyle::SH_Slider_AbsoluteSetButtons, &opt, NULL)) != 0) {
        d->pressedControl = QStyle::SC_SliderHandle;
        setSliderPosition(pressValue);
        triggerAction(SliderMove);
    } else if ((event->button() & style()->styleHint(QStyle::SH_Slider_PageSetButtons, &opt, NULL)) != 0) {
        d->pressedControl = style()->hitTestComplexControl(QStyle::CC_Slider, &opt, event->pos().toPoint(), NULL);
        if (d->pressedControl == QStyle::SC_SliderGroove) {
            d->pressValue = pressValue;
            if (d->pressValue > d->value) {
                triggerAction(SliderPageStepAdd);
                setRepeatAction(SliderPageStepAdd);
            } else if (d->pressValue < d->value) {
                triggerAction(SliderPageStepSub);
                setRepeatAction(SliderPageStepSub);
            }
        }
    } else {
        event->ignore();
        return;
    }

    if (d->pressedControl == QStyle::SC_SliderHandle) {
        setRepeatAction(SliderNoAction);
        d->clickOffset = center;
        setSliderDown(true);
        update();
    }
}

void GraphicsSlider::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    Q_D(GraphicsSlider);

    base_type::mouseMoveEvent(event);

    if (d->pressedControl != QStyle::SC_SliderHandle) {
        event->ignore();
        return;
    }

    event->accept();
    setSliderPosition(valueFromPosition(event->pos().toPoint() - d->clickOffset));
}

void GraphicsSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_D(GraphicsSlider);

    base_type::mouseReleaseEvent(event);

    if (d->pressedControl == QStyle::SC_None || event->buttons()) {
        event->ignore();
        return;
    }

    event->accept();
    QStyle::SubControl oldPressed = QStyle::SubControl(d->pressedControl);
    d->pressedControl = QStyle::SC_None;
    setRepeatAction(SliderNoAction);
    if (oldPressed == QStyle::SC_SliderHandle)
        setSliderDown(false);

    QStyleOptionSlider opt;
    initStyleOption(&opt);
    opt.subControls = oldPressed;
    update(style()->subControlRect(QStyle::CC_Slider, &opt, oldPressed, NULL));
}

void GraphicsSlider::initStyleOption(QStyleOption *option) const
{
    base_type::initStyleOption(option);

    if (QStyleOptionSlider *sliderOption = qstyleoption_cast<QStyleOptionSlider *>(option)) {
        Q_D(const GraphicsSlider);
        sliderOption->subControls = QStyle::SC_None;
        sliderOption->activeSubControls = QStyle::SC_None;
        sliderOption->tickPosition = (QSlider::TickPosition) d->tickPosition;
        sliderOption->tickInterval = d->tickInterval;
    }
}

QSizeF GraphicsSlider::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    if (which == Qt::MinimumSize || which == Qt::PreferredSize) {
        Q_D(const GraphicsSlider);

        QStyleOptionSlider opt;
        initStyleOption(&opt);

        const int SliderLength = 60, TickSpace = 5;
        int thick = style()->pixelMetric(QStyle::PM_SliderThickness, &opt, NULL);
        if (d->tickPosition & TicksAbove)
            thick += TickSpace;
        if (d->tickPosition & TicksBelow)
            thick += TickSpace;
        int w = SliderLength, h = thick;
        if (d->orientation == Qt::Vertical) {
            w = thick;
            h = SliderLength;
        }

        QSizeF sizeHint = QSizeF(style()->sizeFromContents(QStyle::CT_Slider, &opt, QSize(w, h), NULL)
                                 .expandedTo(QApplication::globalStrut()));
        if (which == Qt::MinimumSize) {
            int length = style()->pixelMetric(QStyle::PM_SliderLength, &opt, NULL);
            if (d->orientation == Qt::Horizontal)
                sizeHint.setWidth(length);
            else
                sizeHint.setHeight(length);
        }

        return sizeHint;
    }

    return base_type::sizeHint(which, constraint);
}

void GraphicsSlider::setTickPosition(TickPosition position)
{
    Q_D(GraphicsSlider);
    d->tickPosition = position;
    updateGeometry();
    update();
}

GraphicsSlider::TickPosition GraphicsSlider::tickPosition() const
{
    return d_func()->tickPosition;
}

qint64 GraphicsSlider::tickInterval() const
{
    return d_func()->tickInterval;
}

void GraphicsSlider::setTickInterval(qint64 tickInterval)
{
    d_func()->tickInterval = qMax(0ll, tickInterval);
    update();
}

