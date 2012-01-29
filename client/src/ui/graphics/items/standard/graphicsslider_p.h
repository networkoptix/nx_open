#ifndef GRAPHICSSLIDER_P_H
#define GRAPHICSSLIDER_P_H

#include "abstractgraphicsslider_p.h"
#include "graphicsslider.h"

class GraphicsSliderPrivate : public AbstractGraphicsSliderPrivate
{
    Q_DECLARE_PUBLIC(GraphicsSlider)

public:
    QStyle::SubControl pressedControl;
    GraphicsSlider::TickPosition tickPosition;
    int tickInterval;
    int clickOffset;

    void init();
    int pixelPosToRangeValue(int pos) const;
    inline int pick(const QPoint &pt) const
    { return orientation == Qt::Horizontal ? pt.x() : pt.y(); }

    QStyle::SubControl newHoverControl(const QPoint &pos);
    void updateHoverControl(const QPoint &pos);
    QStyle::SubControl hoverControl;
    QRect hoverRect;
};

#endif // GRAPHICSSLIDER_P_H
