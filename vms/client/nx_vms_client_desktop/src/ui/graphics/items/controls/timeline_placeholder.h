// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <qt_graphics_items/graphics_label.h>

class QnTimeSlider;

class QnTimelinePlaceholder : public GraphicsLabel
{
    Q_OBJECT
    typedef GraphicsLabel base_type;

public:
    QnTimelinePlaceholder(QGraphicsItem* parent, QnTimeSlider* slider);
};
