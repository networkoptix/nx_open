#ifndef ABSTRACTGRAPHICSSLIDER_P_H
#define ABSTRACTGRAPHICSSLIDER_P_H

#include "graphicswidget_p.h"
#include "abstractgraphicsslider.h"

#include <QtCore/QBasicTimer>
#ifdef QT_KEYPAD_NAVIGATION
#  include <QtCore/QElapsedTimer>
#endif

#include <QtGui/QStyle>

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
        repeatAction(AbstractGraphicsSlider::SliderNoAction)
#ifdef QT_KEYPAD_NAVIGATION
      , isAutoRepeating(false)
      , repeatMultiplier(1)
    {
        firstRepeat.invalidate();
#else
    {
#endif
    }

    void setSteps(int single, int page);

    Qt::Orientation orientation;

    int minimum, maximum, pageStep, value, position, pressValue;

    // Call effectiveSingleStep() when changing the slider value.
    int singleStep;

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

#ifdef QT_KEYPAD_NAVIGATION
    int origValue;

    bool isAutoRepeating;

    // When we're auto repeating, we multiply singleStep with this value to
    // get our effective step.
    qreal repeatMultiplier;

    // The time of when the first auto repeating key press event occurs.
    QElapsedTimer firstRepeat;
#endif

    inline int effectiveSingleStep() const
    {
        return singleStep
#ifdef QT_KEYPAD_NAVIGATION
        * repeatMultiplier
#endif
        ;
    }

    inline int overflowSafeAdd(int add) const
    {
        int newValue = value + add;
        if (add > 0 && newValue < value)
            newValue = maximum;
        else if (add < 0 && newValue > value)
            newValue = minimum;
        return newValue;
    }

    inline void setAdjustedSliderPosition(int position)
    {
        Q_Q(AbstractGraphicsSlider);
        if (q->style()->styleHint(QStyle::SH_Slider_StopMouseOverSlider)) {
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
