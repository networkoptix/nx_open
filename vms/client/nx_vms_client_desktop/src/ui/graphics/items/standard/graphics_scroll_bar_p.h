#ifndef GRAPHICSSCROLLBAR_P_H
#define GRAPHICSSCROLLBAR_P_H

#include "abstract_linear_graphics_slider_p.h"

class GraphicsScrollBarPrivate : public AbstractLinearGraphicsSliderPrivate
{
    Q_DECLARE_PUBLIC(GraphicsScrollBar)
public:
    QStyle::SubControl pressedControl;
    bool pointerOutsidePressedControl;
    bool adjustForPressedHandle;

    QPointF relativeClickOffset;
    qint64 snapBackPosition;

    void activateControl(uint control, int threshold = 500);
    void stopRepeatAction();
    void init();
    bool updateHoverControl(const QPointF &pos);
    QStyle::SubControl newHoverControl(const QPointF &pos);

    QStyle::SubControl hoverControl;
    QRect hoverRect;

    void setSliderPositionIgnoringAdjustments(qint64 position);
};

#endif // GRAPHICSSCROLLBAR_P_H

