#ifndef GRAPHICSSLIDER_P_H
#define GRAPHICSSLIDER_P_H

#include "abstract_graphics_slider_p.h"
#include "graphics_slider.h"

class GraphicsSliderPrivate : public AbstractGraphicsSliderPrivate
{
    Q_DECLARE_PUBLIC(GraphicsSlider)

public:
    QStyle::SubControl pressedControl;
    GraphicsSlider::TickPosition tickPosition;
    qint64 tickInterval;
    int clickOffset;

    void init();
    qint64 pixelPosToRangeValue(int pos) const;
    inline int pick(const QPoint &pt) const
    { return orientation == Qt::Horizontal ? pt.x() : pt.y(); }

    QStyle::SubControl newHoverControl(const QPoint &pos);
    void updateHoverControl(const QPoint &pos);
    QStyle::SubControl hoverControl;
    QRect hoverRect;
};

#endif // GRAPHICSSLIDER_P_H
