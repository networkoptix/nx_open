#pragma once

#include "styled_tooltip_widget.h"

class QnSliderTooltipWidget : public QnStyledTooltipWidget
{
    Q_OBJECT
    typedef QnStyledTooltipWidget base_type;

public:
    QnSliderTooltipWidget(QGraphicsItem* parent = nullptr);
};
