#ifndef GRAPHICSSLIDER_P_H
#define GRAPHICSSLIDER_P_H

#include "abstract_linear_graphics_slider_p.h"
#include "graphics_slider.h"

class GraphicsSliderPrivate : public AbstractLinearGraphicsSliderPrivate
{
    Q_DECLARE_PUBLIC(GraphicsSlider)

public:
    QStyle::SubControl pressedControl;
    GraphicsSlider::TickPosition tickPosition;
    qint64 tickInterval;
    QPoint clickOffset;

    void init();
    inline int pick(const QPoint &pt) const
    { return orientation == Qt::Horizontal ? pt.x() : pt.y(); }

    QStyle::SubControl newHoverControl(const QPoint &pos);
    void updateHoverControl(const QPoint &pos);
    QStyle::SubControl hoverControl;
    QRect hoverRect;
};

#endif // GRAPHICSSLIDER_P_H
