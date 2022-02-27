// Modified by Network Optix, Inc. Original copyright notice follows.

/****************************************************************************
**
** Copyright (C) 1992-2005 Trolltech AS. All rights reserved.
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://www.trolltech.com/products/qt/opensource.html
**
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://www.trolltech.com/products/qt/licensing.html or contact the
** sales department at sales@trolltech.com.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

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
        wheelFactor(1.0)
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

    int m_skipUpdateMask = 0;

    void setSkipUpdateOnSliderChange(const QSet<AbstractGraphicsSlider::SliderChange>& set)
    {
        m_skipUpdateMask = 0;
        for (auto flag : set)
            m_skipUpdateMask |= (1 << flag);
    }

    bool canSkipUpdate(AbstractGraphicsSlider::SliderChange change) const
    {
        return ((1 << change) & m_skipUpdateMask);
    }

    void mouseEvent(QGraphicsSceneMouseEvent *event)
    {
        mouseWidget = event->widget();
        mouseScreenPos = event->screenPos();
        mouseButtons = event->buttons();
    }

    qint64 effectiveSingleStep() const
    {
        return singleStep
#ifdef QT_KEYPAD_NAVIGATION
        * repeatMultiplier
#endif
        ;
    }

    void setAdjustedSliderPosition(qint64 position)
    {
        Q_Q(AbstractGraphicsSlider);
        if (q->style()->styleHint(QStyle::SH_Slider_StopMouseOverSlider, nullptr, nullptr)) {
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
