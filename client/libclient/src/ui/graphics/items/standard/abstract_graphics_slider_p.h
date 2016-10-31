#ifndef ABSTRACTGRAPHICSSLIDER_P_H
#define ABSTRACTGRAPHICSSLIDER_P_H

#include "graphics_widget_p.h"
#include "abstract_graphics_slider.h"

#include <QtWidgets/QWidget>
#include <QtCore/QBasicTimer>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#ifdef QT_KEYPAD_NAVIGATION
#  include <QtCore/QElapsedTimer>
#endif

#include <QtWidgets/QStyle>

class AbstractGraphicsSliderPrivate: public GraphicsWidgetPrivate
{
    Q_DECLARE_PUBLIC(AbstractGraphicsSlider)

public:
    AbstractGraphicsSliderPrivate():
        orientation(Qt::Horizontal),
        minimum(0), maximum(99), pageStep(10), value(0), position(0), pressValue(-1),
        singleStep(1), offset_accumulated(0), tracking(true),
        blockTracking(false), pressed(false),
        invertedAppearance(false), invertedControls(false),
        acceleratedWheeling(false),
        repeatAction(AbstractGraphicsSlider::SliderNoAction),
        wheelFactor(1.0),
        mouseButtons(0)
#ifdef QT_KEYPAD_NAVIGATION
      , isAutoRepeating(false)
      , repeatMultiplier(1)
    {
        firstRepeat.invalidate();
#else
    {
#endif
    }

    void setSteps(qint64 single, qint64 page);

    Qt::Orientation orientation;

    qint64 minimum, maximum, pageStep, value, position, pressValue;

    // Call effectiveSingleStep() when changing the slider value.
    qint64 singleStep;

    float offset_accumulated;
    uint tracking            : 1;
    uint blockTracking       : 1;
    uint pressed             : 1;
    uint invertedAppearance  : 1;
    uint invertedControls    : 1;
    uint acceleratedWheeling : 1;

    QBasicTimer repeatActionTimer;
    int repeatActionTime;
    AbstractGraphicsSlider::SliderAction repeatAction;

    qreal wheelFactor;

#ifdef QT_KEYPAD_NAVIGATION
    qint64 origValue;

    bool isAutoRepeating;

    // When we're auto repeating, we multiply singleStep with this value to
    // get our effective step.
    qreal repeatMultiplier;

    // The time of when the first auto repeating key press event occurs.
    QElapsedTimer firstRepeat;
#endif

    QPointer<QWidget> mouseWidget;
    Qt::MouseButtons mouseButtons;
    QPoint mouseScreenPos;

    inline void mouseEvent(QGraphicsSceneMouseEvent *event) {
        mouseWidget = event->widget();
        mouseScreenPos = event->screenPos();
        mouseButtons = event->buttons();
    }

    inline qint64 effectiveSingleStep() const
    {
        return singleStep
#ifdef QT_KEYPAD_NAVIGATION
        * repeatMultiplier
#endif
        ;
    }

    inline void setAdjustedSliderPosition(qint64 position)
    {
        Q_Q(AbstractGraphicsSlider);
        if (q->style()->styleHint(QStyle::SH_Slider_StopMouseOverSlider, NULL, NULL)) {
            if ((position > pressValue - 2 * pageStep) && (position < pressValue + 2 * pageStep)) {
                repeatAction = AbstractGraphicsSlider::SliderNoAction;
                q->setSliderPosition(pressValue);
                return;
            }
        }
        q->triggerAction(repeatAction);
    }

    bool scrollByDelta(Qt::Orientation orientation, Qt::KeyboardModifiers modifiers, int delta);
};

#endif // ABSTRACTGRAPHICSSLIDER_P_H
