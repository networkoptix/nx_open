#pragma once

#include <ui/graphics/items/standard/graphics_label.h>

class QnTimeSlider;

class QnTimelinePlaceholder : public GraphicsLabel
{
    Q_OBJECT
    typedef GraphicsLabel base_type;

public:
    QnTimelinePlaceholder(QGraphicsItem* parent, QnTimeSlider* slider);
};
