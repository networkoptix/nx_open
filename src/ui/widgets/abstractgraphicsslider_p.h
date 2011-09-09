#ifndef ABSTRACTGRAPHICSSLIDER_P_H
#define ABSTRACTGRAPHICSSLIDER_P_H

#include "abstractgraphicsslider.h"

#include <QtCore/QBasicTimer>
#include <QtCore/QElapsedTimer>

#include <QtGui/QStyle>

class AbstractGraphicsSliderPrivate
{
    Q_DECLARE_PUBLIC(AbstractGraphicsSlider)

public:
    AbstractGraphicsSliderPrivate();
    ~AbstractGraphicsSliderPrivate();

    AbstractGraphicsSlider *q_ptr;

    // QTBUG-18797: When setting the flag ItemIgnoresTransformations for an item,
    // it will receive mouse events as if it was transformed by the view.
    bool isUnderMouse;

    void setSteps(int single, int page);

    int minimum, maximum, pageStep, value, position, pressValue;

    // Call effectiveSingleStep() when changing the slider value.
    int singleStep;

    float offset_accumulated;
    uint tracking : 1;
    uint blocktracking :1;
    uint pressed : 1;
    uint invertedAppearance : 1;
    uint invertedControls : 1;
    Qt::Orientation orientation;

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

    virtual int bound(int val) const { return qMax(minimum, qMin(maximum, val)); }
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
    int stepsToScrollForDelta(Qt::Orientation orientation, Qt::KeyboardModifiers modifiers, int delta);
};

#endif // ABSTRACTGRAPHICSSLIDER_P_H
