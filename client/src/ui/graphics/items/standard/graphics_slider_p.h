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
    QPoint clickOffset;

    void init();
    inline int pick(const QPoint &pt) const
    { return orientation == Qt::Horizontal ? pt.x() : pt.y(); }

    QStyle::SubControl newHoverControl(const QPoint &pos);
    void updateHoverControl(const QPoint &pos);
    QStyle::SubControl hoverControl;
    QRect hoverRect;

    void invalidateMapper();

    mutable bool mapperDirty;
    mutable bool upsideDown;
    mutable qreal pixelPosMin, pixelPosMax;
    void ensureMapper() const;
};

#endif // GRAPHICSSLIDER_P_H
