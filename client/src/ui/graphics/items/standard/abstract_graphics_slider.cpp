#include "abstract_graphics_slider.h"
#include "abstract_graphics_slider_p.h"

#ifndef QT_NO_ACCESSIBILITY
#  include <QtGui/QAccessible>
#endif
#include <QtGui/QApplication>
#include <QtGui/QGraphicsSceneEvent>
#include <QtGui/QStyleOption>

namespace {
    bool isInt(qint64 value) {
        return value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max();
    }

} // anonymous namespace

/*!
    \class AbstractGraphicsSlider
    \brief The AbstractGraphicsSlider class provides an integer value within a range.

    The class is designed as a common super class for widgets like GraphicsSlider.

    Here are the main properties of the class:

    \list 1

    \i \l value: The bounded integer that AbstractGraphicsSlider maintains.

    \i \l minimum: The lowest possible value.

    \i \l maximum: The highest possible value.

    \i \l singleStep: The smaller of two natural steps that an abstract sliders
          provides and typically corresponds to the user pressing an arrow key.

    \i \l pageStep: The larger of two natural steps that an abstract slider
          provides and typically corresponds to the user pressing PageUp or PageDown.

    \i \l tracking: Whether slider tracking is enabled.

    \i \l sliderPosition: The current position of the slider. If \l tracking
          is enabled (the default), this is identical to \l value.

    \endlist

    Unity (1) may be viewed as a third step size. setValue() lets you
    set the current value to any integer in the allowed range, not
    just minimum() + \e n * singleStep() for integer values of \e n.
    Some widgets may allow the user to set any value at all; others
    may just provide multiples of singleStep() or pageStep().

    AbstractGraphicsSlider emits a comprehensive set of signals:

    \table
    \header \i Signal \i Emitted when
    \row \i \l valueChanged()
         \i the value has changed. The \l tracking determines whether
            this signal is emitted during user interaction.
    \row \i \l sliderPressed()
         \i the user starts to drag the slider.
    \row \i \l sliderMoved()
         \i the user drags the slider.
    \row \i \l sliderReleased()
         \i the user releases the slider.
    \row \i \l actionTriggered()
         \i a slider action was triggerd.
    \row \i \l rangeChanged()
         \i a the range has changed.
    \endtable

    AbstractGraphicsSlider provides a virtual sliderChange() function that is
    well suited for updating the on-screen representation of
    sliders. By calling triggerAction(), subclasses trigger slider
    actions. Two helper functions QStyle::sliderPositionFromValue() and
    QStyle::sliderValueFromPosition() help subclasses and styles to map
    screen coordinates to logical range values.

    \sa GraphicsSlider
*/

/*!
    \enum AbstractGraphicsSlider::SliderAction

    \value SliderNoAction
    \value SliderSingleStepAdd
    \value SliderSingleStepSub
    \value SliderPageStepAdd
    \value SliderPageStepSub
    \value SliderToMinimum
    \value SliderToMaximum
    \value SliderMove

*/

/*!
    \fn void AbstractGraphicsSlider::valueChanged(int value)

    This signal is emitted when the slider value has changed, with the
    new slider \a value as argument.
*/

/*!
    \fn void AbstractGraphicsSlider::sliderPressed()

    This signal is emitted when the user presses the slider with the
    mouse, or programmatically when setSliderDown(true) is called.

    \sa sliderReleased(), sliderMoved(), isSliderDown()
*/

/*!
    \fn void AbstractGraphicsSlider::sliderMoved(int value)

    This signal is emitted when sliderDown is true and the slider moves. This
    usually happens when the user is dragging the slider. The \a value
    is the new slider position.

    This signal is emitted even when tracking is turned off.

    \sa tracking, valueChanged(), sliderDown, sliderPressed(), sliderReleased()
*/

/*!
    \fn void AbstractGraphicsSlider::sliderReleased()

    This signal is emitted when the user releases the slider with the
    mouse, or programmatically when setSliderDown(false) is called.

    \sa sliderPressed(), sliderMoved(), sliderDown
*/

/*!
    \fn void AbstractGraphicsSlider::rangeChanged(int min, int max)

    This signal is emitted when the slider range has changed, with \a
    min being the new minimum, and \a max being the new maximum.

    \sa minimum, maximum
*/

/*!
    \fn void AbstractGraphicsSlider::actionTriggered(int action)

    This signal is emitted when the slider action \a action is
    triggered. Actions are \l SliderSingleStepAdd, \l
    SliderSingleStepSub, \l SliderPageStepAdd, \l SliderPageStepSub,
    \l SliderToMinimum, \l SliderToMaximum, and \l SliderMove.

    When the signal is emitted, the \l sliderPosition has been
    adjusted according to the action, but the \l value has not yet
    been propagated (meaning the valueChanged() signal was not yet
    emitted), and the visual display has not been updated. In slots
    connected to this signal you can thus safely adjust any action by
    calling setSliderPosition() yourself, based on both the action and
    the slider's value.

    \sa triggerAction()
*/

/*!
    \enum AbstractGraphicsSlider::SliderChange

    \value SliderRangeChange
    \value SliderOrientationChange
    \value SliderStepsChange
    \value SliderValueChange
*/

void AbstractGraphicsSliderPrivate::setSteps(qint64 single, qint64 page)
{
    Q_Q(AbstractGraphicsSlider);
    singleStep = qAbs(single);
    pageStep = qAbs(page);
    q->sliderChange(AbstractGraphicsSlider::SliderStepsChange);
}


/*!
    Constructs an abstract slider.

    The \a parent argument is sent to the QWidget constructor.

    The \l minimum defaults to 0, the \l maximum to 99, with a \l singleStep
    size of 1 and a \l pageStep size of 10, and an initial \l value of 0.
*/
AbstractGraphicsSlider::AbstractGraphicsSlider(QGraphicsItem *parent)
    : GraphicsWidget(*new AbstractGraphicsSliderPrivate, parent)
{
}

/*!
    \internal
*/
AbstractGraphicsSlider::AbstractGraphicsSlider(AbstractGraphicsSliderPrivate &dd, QGraphicsItem *parent)
    : GraphicsWidget(dd, parent)
{
}

/*!
    Destroys the slider.
*/
AbstractGraphicsSlider::~AbstractGraphicsSlider()
{
}

/*!
    \property AbstractGraphicsSlider::orientation
    \brief the orientation of the slider

    The orientation must be \l Qt::Horizontal (the default) or \l Qt::Vertical.
*/
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

/*!
    \property AbstractGraphicsSlider::minimum
    \brief the sliders's minimum value

    When setting this property, the \l maximum is adjusted if
    necessary to ensure that the range remains valid. Also the
    slider's current value is adjusted to be within the new range.
*/
qint64 AbstractGraphicsSlider::minimum() const
{
    return d_func()->minimum;
}

void AbstractGraphicsSlider::setMinimum(qint64 min)
{
    setRange(min, qMax(d_func()->maximum, min));
}

/*!
    \property AbstractGraphicsSlider::maximum
    \brief the slider's maximum value

    When setting this property, the \l minimum is adjusted if
    necessary to ensure that the range remains valid.  Also the
    slider's current value is adjusted to be within the new range.
*/
qint64 AbstractGraphicsSlider::maximum() const
{
    return d_func()->maximum;
}

void AbstractGraphicsSlider::setMaximum(qint64 max)
{
    setRange(qMin(d_func()->minimum, max), max);
}

/*!
    Sets the slider's minimum to \a min and its maximum to \a max.

    If \a max is smaller than \a min, \a min becomes the only legal value.

    \sa minimum, maximum
*/
void AbstractGraphicsSlider::setRange(qint64 min, qint64 max)
{
    Q_D(AbstractGraphicsSlider);
    qint64 oldMin = d->minimum;
    qint64 oldMax = d->maximum;
    d->minimum = min;
    d->maximum = qMax(min, max);
    if (oldMin != d->minimum || oldMax != d->maximum) {
        sliderChange(SliderRangeChange);
        Q_EMIT rangeChanged(d->minimum, d->maximum);
        setValue(d->value); // re-bound
    }
}

/*!
    \property AbstractGraphicsSlider::singleStep
    \brief the single step.

    The smaller of two natural steps that an
    abstract sliders provides and typically corresponds to the user
    pressing an arrow key.

    If the property is modified during an auto repeating key event, behavior
    is undefined.

    \sa pageStep
*/
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

/*!
    \property AbstractGraphicsSlider::pageStep
    \brief the page step.

    The larger of two natural steps that an abstract slider provides
    and typically corresponds to the user pressing PageUp or PageDown.

    \sa singleStep
*/
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

/*!
    \property AbstractGraphicsSlider::tracking
    \brief whether slider tracking is enabled

    If tracking is enabled (the default), the slider emits the
    valueChanged() signal while the slider is being dragged. If
    tracking is disabled, the slider emits the valueChanged() signal
    only when the user releases the slider.

    \sa sliderDown
*/
bool AbstractGraphicsSlider::hasTracking() const
{
    return d_func()->tracking;
}

void AbstractGraphicsSlider::setTracking(bool enable)
{
    d_func()->tracking = enable;
}

/*!
    \property AbstractGraphicsSlider::sliderDown
    \brief whether the slider is pressed down.

    The property is set by subclasses in order to let the abstract
    slider know whether or not \l tracking has any effect.

    Changing the slider down property emits the sliderPressed() and
    sliderReleased() signals.
*/
bool AbstractGraphicsSlider::isSliderDown() const
{
    return d_func()->pressed;
}

void AbstractGraphicsSlider::setSliderDown(bool down)
{
    Q_D(AbstractGraphicsSlider);
    bool doEmit = d->pressed != down;

    d->pressed = down;

    if (doEmit) {
        if (down)
            Q_EMIT sliderPressed();
        else
            Q_EMIT sliderReleased();
    }

    if (!down && d->position != d->value)
        triggerAction(SliderMove);
}

/*!
    \property AbstractGraphicsSlider::sliderPosition
    \brief the current slider position

    If \l tracking is enabled (the default), this is identical to \l value.
*/
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

/*!
    \property AbstractGraphicsSlider::value
    \brief the slider's current value

    The slider forces the value to be within the legal range: \l
    minimum <= \c value <= \l maximum.

    Changing the value also changes the \l sliderPosition.
*/
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
    if (d->position != d->value) {
        d->position = d->value;
        if (d->pressed)
            Q_EMIT sliderMoved(d->position);
    }
#ifndef QT_NO_ACCESSIBILITY
    QAccessible::updateAccessibility(this, 0, QAccessible::ValueChanged);
#endif
    sliderChange(SliderValueChange);
    Q_EMIT valueChanged(d->value);
}

/*!
    \property AbstractGraphicsSlider::invertedAppearance
    \brief whether or not a slider shows its values inverted.

    If this property is false (the default), the minimum and maximum will
    be shown in its classic position for the inherited widget. If the
    value is true, the minimum and maximum appear at their opposite location.

    Note: This property makes most sense for sliders and dials. For
    scroll bars, the visual effect of the scroll bar subcontrols depends on
    whether or not the styles understand inverted appearance; most styles
    ignore this property for scroll bars.
*/
bool AbstractGraphicsSlider::invertedAppearance() const
{
    return d_func()->invertedAppearance;
}

void AbstractGraphicsSlider::setInvertedAppearance(bool invert)
{
    d_func()->invertedAppearance = invert;
    update();
}

/*!
    \property AbstractGraphicsSlider::invertedControls
    \brief whether or not the slider inverts its wheel and key events.

    If this property is false, scrolling the mouse wheel "up" and using keys
    like page up will increase the slider's value towards its maximum. Otherwise
    pressing page up will move value towards the slider's minimum.
*/
bool AbstractGraphicsSlider::invertedControls() const
{
    return d_func()->invertedControls;
}

void AbstractGraphicsSlider::setInvertedControls(bool invert)
{
    d_func()->invertedControls = invert;
}

/*!
    \property AbstractGraphicsSlider::acceleratedWheeling
    \brief whether or not the scrolling the mouse wheel leads to accelerated moving the value.

    If this property is false, scrolling the mouse wheel won't increase
    the slider's value more than one page in any case. Otherwise
    scrolling the mouse wheel will accelerate moving the value.
*/
bool AbstractGraphicsSlider::isWheelingAccelerated() const
{
    return d_func()->acceleratedWheeling;
}

void AbstractGraphicsSlider::setWheelingAccelerated(bool enable)
{
    d_func()->acceleratedWheeling = enable;
}

/*!
    Triggers a slider \a action.

    \sa actionTriggered()
*/
void AbstractGraphicsSlider::triggerAction(SliderAction action)
{
    Q_D(AbstractGraphicsSlider);
    d->blockTracking = true;
    switch (action) {
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

/*!
    Returns the current repeat action.

    \sa setRepeatAction()
*/
AbstractGraphicsSlider::SliderAction AbstractGraphicsSlider::repeatAction() const
{
    return d_func()->repeatAction;
}

/*!
    Sets action \a action to be triggered repetitively in intervals
    of \a repeatTime, after an initial delay of \a thresholdTime.

    \sa triggerAction() repeatAction()
*/
void AbstractGraphicsSlider::setRepeatAction(SliderAction action, int thresholdTime, int repeatTime)
{
    Q_D(AbstractGraphicsSlider);
    if ((d->repeatAction = action) == SliderNoAction) {
        d->repeatActionTimer.stop();
    } else {
        d->repeatActionTime = repeatTime;
        d->repeatActionTimer.start(thresholdTime, this);
    }
}

/*!
    Reimplement this virtual function to track slider changes such as
    \l SliderRangeChange, \l SliderOrientationChange, \l SliderStepsChange,
    or \l SliderValueChange. The default implementation only updates the display
    and ignores the \a change parameter.
*/
void AbstractGraphicsSlider::sliderChange(SliderChange)
{
    update();
}

bool AbstractGraphicsSliderPrivate::scrollByDelta(Qt::Orientation orientation, Qt::KeyboardModifiers modifiers, int delta)
{
    Q_Q(AbstractGraphicsSlider);
    qint64 stepsToScroll = 0;
    // in Qt scrolling to the right gives negative values.
    if (orientation == Qt::Horizontal)
        delta = -delta;
    qreal offset = qreal(delta) / 120;

    if (modifiers & (Qt::ControlModifier | Qt::ShiftModifier)) {
        // Scroll one page regardless of delta:
        stepsToScroll = qBound(-pageStep, qint64(offset * pageStep), pageStep);
        offset_accumulated = 0;
    } else {
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
        if (!acceleratedWheeling) {
            // Don't scroll more than one page in any case
            stepsToScroll = qBound(-pageStep, qint64(offset_accumulated), pageStep);
        } else {
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

    if (prevPosition == position) {
        offset_accumulated = 0;
        return false;
    }

    return true;
}

/*!
    \reimp
*/
void AbstractGraphicsSlider::keyPressEvent(QKeyEvent *event)
{
    Q_D(AbstractGraphicsSlider);

    SliderAction action = SliderNoAction;
#ifdef QT_KEYPAD_NAVIGATION
    if (event->isAutoRepeat()) {
        if (!d->firstRepeat.isValid())
            d->firstRepeat.start();
        else if (1 == d->repeatMultiplier) {
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
    } else if (d->firstRepeat.isValid()) {
        d->firstRepeat.invalidate();
        d->repeatMultiplier = 1;
    }
#endif

    switch (event->key()) {
#ifdef QT_KEYPAD_NAVIGATION
        case Qt::Key_Select:
            if (QApplication::keypadNavigationEnabled())
                setEditFocus(!hasEditFocus());
            else
                event->ignore();
            break;
        case Qt::Key_Back:
            if (QApplication::keypadNavigationEnabled() && hasEditFocus()) {
                setValue(d->origValue);
                setEditFocus(false);
            } else
                event->ignore();
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
                    && (QWidgetPrivate::canKeypadNavigate(Qt::Horizontal) || QWidgetPrivate::inTabWidget(this)))) {
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
                    && (QWidgetPrivate::canKeypadNavigate(Qt::Horizontal) || QWidgetPrivate::inTabWidget(this)))) {
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
                    || !hasEditFocus() && QWidgetPrivate::canKeypadNavigate(Qt::Vertical))) {
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

/*!
    \reimp
*/
void AbstractGraphicsSlider::initStyleOption(QStyleOption *option) const
{
    Q_D(const AbstractGraphicsSlider);

    GraphicsWidget::initStyleOption(option);

    if (d->orientation == Qt::Horizontal)
        option->state |= QStyle::State_Horizontal;

    if (QStyleOptionSlider *sliderOption = qstyleoption_cast<QStyleOptionSlider *>(option)) {
        sliderOption->subControls = QStyle::SC_None;
        sliderOption->activeSubControls = QStyle::SC_None;
        sliderOption->orientation = d->orientation;
        sliderOption->upsideDown = d->orientation == Qt::Horizontal
                                   ? d->invertedAppearance != (option->direction == Qt::RightToLeft)
                                   : !d->invertedAppearance;
        sliderOption->direction = Qt::LeftToRight; // we use the upsideDown option instead
        
        if(isInt(d->maximum) && isInt(d->minimum)) {
            sliderOption->maximum = d->maximum;
            sliderOption->minimum = d->minimum;
            sliderOption->sliderPosition = d->position;
            sliderOption->sliderValue = d->value;
            sliderOption->singleStep = d->singleStep;
            sliderOption->pageStep = d->pageStep;
        } else {
            qint64 k = qMax(qAbs(d->maximum), qAbs(d->minimum)) / (std::numeric_limits<int>::max() / 2) + 1;

            sliderOption->maximum           = d->maximum / k;
            sliderOption->minimum           = d->minimum / k;
            sliderOption->sliderPosition    = d->position / k;
            sliderOption->sliderValue       = d->value / k;
            sliderOption->singleStep        = d->singleStep / k;
            sliderOption->pageStep          = d->pageStep / k;
        }
    }
}

/*!
    \reimp
*/
bool AbstractGraphicsSlider::event(QEvent *event)
{
#ifdef QT_KEYPAD_NAVIGATION
    if (event->type() == QEvent::FocusIn) {
        Q_D(AbstractGraphicsSlider);
        d->origValue = d->value;
    }
#endif

    return GraphicsWidget::event(event);
}

/*!
    \reimp
*/
void AbstractGraphicsSlider::changeEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::EnabledChange:
        if (!isEnabled()) {
            d_func()->repeatActionTimer.stop();
            setSliderDown(false);
        }
        // fall through
    default:
        break;
    }

    GraphicsWidget::changeEvent(event);
}

/*!
    \reimp
*/
void AbstractGraphicsSlider::timerEvent(QTimerEvent *event)
{
    Q_D(AbstractGraphicsSlider);
    if (event->timerId() == d->repeatActionTimer.timerId()) {
        if (d->repeatActionTime) { // was threshold time, use repeat time next time
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
/*!
    \reimp
*/
void AbstractGraphicsSlider::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    event->accept();
    d_func()->scrollByDelta(event->orientation(), event->modifiers(), event->delta());
}
#endif


