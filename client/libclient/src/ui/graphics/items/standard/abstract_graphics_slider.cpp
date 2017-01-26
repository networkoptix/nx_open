#include "abstract_graphics_slider.h"
#include "abstract_graphics_slider_p.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsSceneEvent>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QGraphicsView>
#include <limits>

namespace {

    // Max range for QStyleOptionSlider to avoid overflows and Qt's mishandling:
    const int kStyleRangeLimit = std::numeric_limits<int>::max() / 4;

} // anonymous namespace

void AbstractGraphicsSliderPrivate::setSteps(qint64 single, qint64 page)
{
    Q_Q(AbstractGraphicsSlider);
    single = qAbs(single);
    page = qAbs(page);

    bool singleChanged = singleStep != single;
    bool pageChanged = pageStep != page;

    singleStep = single;
    pageStep = page;

    q->sliderChange(AbstractGraphicsSlider::SliderStepsChange);

    if (singleChanged)
        emit q->singleStepChanged(singleStep);
    if (pageChanged)
        emit q->pageStepChanged(pageStep);
}


AbstractGraphicsSlider::AbstractGraphicsSlider(QGraphicsItem* parent):
    base_type(*new AbstractGraphicsSliderPrivate, parent)
{
}

AbstractGraphicsSlider::AbstractGraphicsSlider(AbstractGraphicsSliderPrivate& dd, QGraphicsItem* parent):
    base_type(dd, parent)
{
}

AbstractGraphicsSlider::~AbstractGraphicsSlider()
{
}

Qt::Orientation AbstractGraphicsSlider::orientation() const
{
    return d_func()->orientation;
}

void AbstractGraphicsSlider::setOrientation(Qt::Orientation orientation)
{
    Q_D(AbstractGraphicsSlider);
    if (d->orientation == orientation)
        return;

    d->orientation = orientation;

    QSizePolicy sp = sizePolicy();
    sp.transpose();
    setSizePolicy(sp);

    update();
    updateGeometry();
}

qint64 AbstractGraphicsSlider::minimum() const
{
    return d_func()->minimum;
}

void AbstractGraphicsSlider::setMinimum(qint64 min)
{
    setRange(min, qMax(d_func()->maximum, min));
}

qint64 AbstractGraphicsSlider::maximum() const
{
    return d_func()->maximum;
}

void AbstractGraphicsSlider::setMaximum(qint64 max)
{
    setRange(qMin(d_func()->minimum, max), max);
}

void AbstractGraphicsSlider::setRange(qint64 min, qint64 max)
{
    Q_D(AbstractGraphicsSlider);
    qint64 oldMin = d->minimum;
    qint64 oldMax = d->maximum;
    d->minimum = min;
    d->maximum = qMax(min, max);

    if (oldMin != d->minimum || oldMax != d->maximum)
    {
        sliderChange(SliderRangeChange);
        Q_EMIT rangeChanged(d->minimum, d->maximum);
        setValue(d->value); // re-bound
    }
}

qint64 AbstractGraphicsSlider::singleStep() const
{
    return d_func()->singleStep;
}

void AbstractGraphicsSlider::setSingleStep(qint64 step)
{
    Q_D(AbstractGraphicsSlider);
    if (step != d->singleStep)
        d->setSteps(step, d->pageStep);
}

qint64 AbstractGraphicsSlider::pageStep() const
{
    return d_func()->pageStep;
}

void AbstractGraphicsSlider::setPageStep(qint64 step)
{
    Q_D(AbstractGraphicsSlider);
    if (step != d->pageStep)
        d->setSteps(d->singleStep, step);
}

bool AbstractGraphicsSlider::hasTracking() const
{
    return d_func()->tracking;
}

void AbstractGraphicsSlider::setTracking(bool enable)
{
    d_func()->tracking = enable;
}

bool AbstractGraphicsSlider::isSliderDown() const
{
    return d_func()->pressed;
}

void AbstractGraphicsSlider::setSliderDown(bool down)
{
    Q_D(AbstractGraphicsSlider);
    bool doEmit = d->pressed != down;

    d->pressed = down;

    if (doEmit)
    {
        if (down)
            Q_EMIT sliderPressed();
        else
            Q_EMIT sliderReleased();
    }

    if (!down && d->position != d->value)
        triggerAction(SliderMove);
}

qint64 AbstractGraphicsSlider::sliderPosition() const
{
    return d_func()->position;
}

void AbstractGraphicsSlider::setSliderPosition(qint64 position)
{
    Q_D(AbstractGraphicsSlider);
    position = qBound(d->minimum, position, d->maximum);
    if (position == d->position)
        return;

    d->position = position;
    if (!d->tracking)
        update();

    if (d->pressed)
        Q_EMIT sliderMoved(position);

    if (d->tracking && !d->blockTracking)
        triggerAction(SliderMove);
}

qint64 AbstractGraphicsSlider::value() const
{
    return d_func()->value;
}

void AbstractGraphicsSlider::setValue(qint64 value)
{
    Q_D(AbstractGraphicsSlider);
    value = qBound(d->minimum, value, d->maximum);
    if (d->value == value && d->position == value)
        return;

    d->value = value;
    if (d->position != d->value)
    {
        d->position = d->value;
        if (d->pressed)
            Q_EMIT sliderMoved(d->position);
    }

    sliderChange(SliderValueChange);
    Q_EMIT valueChanged(d->value);
}

bool AbstractGraphicsSlider::invertedAppearance() const
{
    return d_func()->invertedAppearance;
}

void AbstractGraphicsSlider::setInvertedAppearance(bool invert)
{
    d_func()->invertedAppearance = invert;
    update();
}

bool AbstractGraphicsSlider::invertedControls() const
{
    return d_func()->invertedControls;
}

void AbstractGraphicsSlider::setInvertedControls(bool invert)
{
    d_func()->invertedControls = invert;
}

bool AbstractGraphicsSlider::isWheelingAccelerated() const
{
    return d_func()->acceleratedWheeling;
}

void AbstractGraphicsSlider::setWheelingAccelerated(bool enable)
{
    d_func()->acceleratedWheeling = enable;
}

qreal AbstractGraphicsSlider::wheelFactor() const
{
    return d_func()->wheelFactor;
}

void AbstractGraphicsSlider::setWheelFactor(qreal factor)
{
    d_func()->wheelFactor = factor;
}

void AbstractGraphicsSlider::triggerAction(SliderAction action)
{
    Q_D(AbstractGraphicsSlider);
    d->blockTracking = true;

    switch (action)
    {
        case SliderSingleStepAdd:
            setSliderPosition(d->value + d->effectiveSingleStep());
            break;

        case SliderSingleStepSub:
            setSliderPosition(d->value - d->effectiveSingleStep());
            break;

        case SliderPageStepAdd:
            setSliderPosition(d->value + d->pageStep);
            break;

        case SliderPageStepSub:
            setSliderPosition(d->value - d->pageStep);
            break;

        case SliderToMinimum:
            setSliderPosition(d->minimum);
            break;

        case SliderToMaximum:
            setSliderPosition(d->maximum);
            break;

        case SliderMove:
        case SliderNoAction:
            break;
    };

    Q_EMIT actionTriggered(action);
    d->blockTracking = false;
    setValue(d->position);
}

AbstractGraphicsSlider::SliderAction AbstractGraphicsSlider::repeatAction() const
{
    return d_func()->repeatAction;
}

void AbstractGraphicsSlider::setRepeatAction(SliderAction action, int thresholdTime, int repeatTime)
{
    Q_D(AbstractGraphicsSlider);
    if ((d->repeatAction = action) == SliderNoAction)
    {
        d->repeatActionTimer.stop();
    } else
    {
        d->repeatActionTime = repeatTime;
        d->repeatActionTimer.start(thresholdTime, this);
    }
}

void AbstractGraphicsSlider::sliderChange(SliderChange)
{
    update();
}

void AbstractGraphicsSlider::sendPendingMouseMoves(bool checkPosition)
{
    sendPendingMouseMoves(d_func()->mouseWidget.data(), checkPosition);
}

void AbstractGraphicsSlider::sendPendingMouseMoves(QWidget* widget, bool checkPosition)
{
    Q_D(AbstractGraphicsSlider);

    if (!scene() || scene()->mouseGrabberItem() != this)
        return;

    if (!widget || widget != d->mouseWidget.data())
        return;

    /* We send only 'repeating' mouse events that do not change current buttons state.
     * Otherwise, synthetic mouse events would end up interfering with the real ones,
     * which may lead to some surprising effects. */
    Qt::MouseButtons buttons = QApplication::mouseButtons();
    if (buttons == 0 || buttons != d->mouseButtons)
        return;

    QPoint pos = QCursor::pos();
    if (checkPosition && pos == d->mouseScreenPos)
        return;

    QGraphicsView* view = dynamic_cast<QGraphicsView*>(widget->parent());
    if (!view)
        return;

    QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMouseMove);
    event.setButtons(buttons);
    event.setScreenPos(QCursor::pos());
    event.setScenePos(view->mapToScene(view->mapFromGlobal(event.screenPos())));
    event.setPos(mapFromScene(event.scenePos()));
    event.setModifiers(QApplication::keyboardModifiers());
    event.setWidget(widget);

    /* We don't set the last position & button down positions because we don't use them.
     * Note that this may lead to problems if derived classes / filters use it. */
    scene()->sendEvent(this, &event);
}

bool AbstractGraphicsSliderPrivate::scrollByDelta(Qt::Orientation orientation, Qt::KeyboardModifiers modifiers, int delta)
{
    Q_Q(AbstractGraphicsSlider);
    qint64 stepsToScroll = 0;
    // in Qt scrolling to the right gives negative values.
    if (orientation == Qt::Horizontal)
        delta = -delta;

    qreal offset = wheelFactor * delta / 120.0;

    if (modifiers & (Qt::ControlModifier | Qt::ShiftModifier))
    {
        // Scroll one page regardless of delta:
        stepsToScroll = qBound(-pageStep, qint64(offset * pageStep), pageStep);
        offset_accumulated = 0;
    }
    else
    {
        // Calculate how many lines to scroll. Depending on what delta is (and
        // offset), we might end up with a fraction (e.g. scroll 1.3 lines). We can
        // only scroll whole lines, so we keep the reminder until next event.
        qreal stepsToScrollF =
#ifndef QT_NO_WHEELEVENT
                QApplication::wheelScrollLines() *
#endif
                offset * effectiveSingleStep();
        // Check if wheel changed direction since last event:
        if (offset_accumulated != 0 && (offset / offset_accumulated) < 0)
            offset_accumulated = 0;

        offset_accumulated += stepsToScrollF;
        if (!acceleratedWheeling)
        {
            // Don't scroll more than one page in any case
            stepsToScroll = qBound(-pageStep, qint64(offset_accumulated), pageStep);
        } else
        {
            // Make it able scroll hundreds of lines at a time as a result of acceleration
            stepsToScroll = qint64(offset_accumulated);
        }

        offset_accumulated -= qint64(offset_accumulated);
        if (stepsToScroll == 0)
            return false;
    }

    if (invertedControls)
        stepsToScroll = -stepsToScroll;

    qint64 prevPosition = position;

    q->setSliderPosition(value + stepsToScroll);

    if (prevPosition == position)
    {
        offset_accumulated = 0;
        return false;
    }

    return true;
}

void AbstractGraphicsSlider::keyPressEvent(QKeyEvent* event)
{
    Q_D(AbstractGraphicsSlider);

    SliderAction action = SliderNoAction;
#ifdef QT_KEYPAD_NAVIGATION
    if (event->isAutoRepeat())
    {
        if (!d->firstRepeat.isValid())
        {
            d->firstRepeat.start();
        }
        else if (1 == d->repeatMultiplier)
        {
            // This is the interval in milli seconds which one key repetition
            // takes.
            const int repeatMSecs = d->firstRepeat.elapsed();

            // The time it takes to currently navigate the whole slider.
            const qreal currentTimeElapse = (qreal(maximum()) / singleStep()) * repeatMSecs;

            // This is an arbitrarily determined constant in msecs that
            // specifies how long time it should take to navigate from the
            // start to the end(excluding starting key auto repeat).
            const int SliderRepeatElapse = 2500;

            d->repeatMultiplier = currentTimeElapse / SliderRepeatElapse;
        }
    }
    else if (d->firstRepeat.isValid())
    {
        d->firstRepeat.invalidate();
        d->repeatMultiplier = 1;
    }
#endif

    switch (event->key())
    {
#ifdef QT_KEYPAD_NAVIGATION
        case Qt::Key_Select:
            if (QApplication::keypadNavigationEnabled())
                setEditFocus(!hasEditFocus());
            else
                event->ignore();
            break;
        case Qt::Key_Back:
            if (QApplication::keypadNavigationEnabled() && hasEditFocus())
            {
                setValue(d->origValue);
                setEditFocus(false);
            }
            else
            {
                event->ignore();
            }
            break;
#endif

        // It seems we need to use invertedAppearance for Left and right, otherwise, things look weird.
        case Qt::Key_Left:
#ifdef QT_KEYPAD_NAVIGATION
            // In QApplication::KeypadNavigationDirectional, we want to change the slider
            // value if there is no left/right navigation possible and if this slider is not
            // inside a tab widget.
            if (QApplication::keypadNavigationEnabled()
                    && (!hasEditFocus() && QApplication::navigationMode() == Qt::NavigationModeKeypadTabOrder
                    || d->orientation == Qt::Vertical
                    || !hasEditFocus()
                    && (QWidgetPrivate::canKeypadNavigate(Qt::Horizontal) || QWidgetPrivate::inTabWidget(this))))
            {
                event->ignore();
                return;
            }

            if (QApplication::keypadNavigationEnabled() && d->orientation == Qt::Vertical)
                action = d->invertedControls ? SliderSingleStepSub : SliderSingleStepAdd;
            else
#endif
            if (testAttribute(Qt::WA_RightToLeft))
                action = d->invertedAppearance ? SliderSingleStepSub : SliderSingleStepAdd;
            else
                action = !d->invertedAppearance ? SliderSingleStepSub : SliderSingleStepAdd;
            break;
        case Qt::Key_Right:
#ifdef QT_KEYPAD_NAVIGATION
            // Same logic as in Qt::Key_Left
            if (QApplication::keypadNavigationEnabled()
                    && (!hasEditFocus() && QApplication::navigationMode() == Qt::NavigationModeKeypadTabOrder
                    || d->orientation == Qt::Vertical
                    || !hasEditFocus()
                    && (QWidgetPrivate::canKeypadNavigate(Qt::Horizontal) || QWidgetPrivate::inTabWidget(this))))
            {
                event->ignore();
                return;
            }
            if (QApplication::keypadNavigationEnabled() && d->orientation == Qt::Vertical)
                action = d->invertedControls ? SliderSingleStepAdd : SliderSingleStepSub;
            else
#endif
            if (testAttribute(Qt::WA_RightToLeft))
                action = d->invertedAppearance ? SliderSingleStepAdd : SliderSingleStepSub;
            else
                action = !d->invertedAppearance ? SliderSingleStepAdd : SliderSingleStepSub;
            break;
        case Qt::Key_Up:
#ifdef QT_KEYPAD_NAVIGATION
            // In QApplication::KeypadNavigationDirectional, we want to change the slider
            // value if there is no up/down navigation possible.
            if (QApplication::keypadNavigationEnabled()
                    && (QApplication::navigationMode() == Qt::NavigationModeKeypadTabOrder
                    || d->orientation == Qt::Horizontal
                    || !hasEditFocus() && QWidgetPrivate::canKeypadNavigate(Qt::Vertical)))
            {
                event->ignore();
                break;
            }
#endif
            action = d->invertedControls ? SliderSingleStepSub : SliderSingleStepAdd;
            break;
        case Qt::Key_Down:
#ifdef QT_KEYPAD_NAVIGATION
            // Same logic as in Qt::Key_Up
            if (QApplication::keypadNavigationEnabled()
                    && (QApplication::navigationMode() == Qt::NavigationModeKeypadTabOrder
                    || d->orientation == Qt::Horizontal
                    || !hasEditFocus() && QWidgetPrivate::canKeypadNavigate(Qt::Vertical))) {
                event->ignore();
                break;
            }
#endif
            action = d->invertedControls ? SliderSingleStepAdd : SliderSingleStepSub;
            break;

        case Qt::Key_PageUp:
            action = d->invertedControls ? SliderPageStepSub : SliderPageStepAdd;
            break;

        case Qt::Key_PageDown:
            action = d->invertedControls ? SliderPageStepAdd : SliderPageStepSub;
            break;

        case Qt::Key_Home:
            action = SliderToMinimum;
            break;

        case Qt::Key_End:
            action = SliderToMaximum;
            break;

        default:
            event->ignore();
            break;
    }
    if (action)
        triggerAction(action);
}

void AbstractGraphicsSlider::initStyleOption(QStyleOption* option) const
{
    Q_D(const AbstractGraphicsSlider);

    base_type::initStyleOption(option);

    if (d->orientation == Qt::Horizontal)
        option->state |= QStyle::State_Horizontal;

    if (QStyleOptionSlider *sliderOption = qstyleoption_cast<QStyleOptionSlider*>(option))
    {
        sliderOption->subControls = QStyle::SC_None;
        sliderOption->activeSubControls = QStyle::SC_None;
        sliderOption->orientation = d->orientation;

        sliderOption->upsideDown = d->orientation == Qt::Horizontal
            ? d->invertedAppearance != (option->direction == Qt::RightToLeft)
            : !d->invertedAppearance;

        sliderOption->direction = Qt::LeftToRight; // we use the upsideDown option instead

        qint64 range = d->maximum - d->minimum;
        NX_ASSERT(range >= 0);

        range += qMax(d->singleStep, d->pageStep);

        qint64 base = 0;
        qreal scale = 1.0;

        if (range > kStyleRangeLimit)
        {
            base = d->minimum;
            scale = static_cast<qreal>(kStyleRangeLimit) / range;
        }
        else if (d->minimum < -kStyleRangeLimit || d->maximum > kStyleRangeLimit)
        {
            base = d->minimum;
        }

        auto scaled =
            [scale](qint64 value, qint64 base = 0) -> int
            {
                return static_cast<int>((value - base) * scale);
            };

        sliderOption->minimum = scaled(d->minimum, base);
        sliderOption->maximum = scaled(d->maximum, base);
        sliderOption->sliderPosition = scaled(d->position, base);
        sliderOption->sliderValue = scaled(d->value, base);
        sliderOption->singleStep = d->singleStep == 0 ? 0 : qMax(scaled(d->singleStep), 1);
        sliderOption->pageStep = d->pageStep == 0 ? 0 : qMax(scaled(d->pageStep), 1);
    }
}

bool AbstractGraphicsSlider::event(QEvent* event)
{
#ifdef QT_KEYPAD_NAVIGATION
    if (event->type() == QEvent::FocusIn)
    {
        Q_D(AbstractGraphicsSlider);
        d->origValue = d->value;
    }
#endif

    return base_type::event(event);
}

void AbstractGraphicsSlider::changeEvent(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::EnabledChange:
            if (!isEnabled())
            {
                d_func()->repeatActionTimer.stop();
                setSliderDown(false);
            }
            // fall through
        default:
            break;
    }

    base_type::changeEvent(event);
}

void AbstractGraphicsSlider::timerEvent(QTimerEvent* event)
{
    Q_D(AbstractGraphicsSlider);
    if (event->timerId() == d->repeatActionTimer.timerId())
    {
        if (d->repeatActionTime) // was threshold time, use repeat time next time
        {
            d->repeatActionTimer.start(d->repeatActionTime, this);
            d->repeatActionTime = 0;
        }

        if (d->repeatAction == SliderPageStepAdd)
            d->setAdjustedSliderPosition(d->value + d->pageStep);
        else if (d->repeatAction == SliderPageStepSub)
            d->setAdjustedSliderPosition(d->value - d->pageStep);
        else
            triggerAction(d->repeatAction);
    }
}

#ifndef QT_NO_WHEELEVENT
void AbstractGraphicsSlider::wheelEvent(QGraphicsSceneWheelEvent* event)
{
    event->accept();
    d_func()->scrollByDelta(event->orientation(), event->modifiers(), event->delta());
}
#endif

void AbstractGraphicsSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    d_func()->mouseEvent(event);
}

void AbstractGraphicsSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    d_func()->mouseEvent(event);
}

void AbstractGraphicsSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    d_func()->mouseEvent(event);
}
