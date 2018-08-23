#pragma once

#include "styled_tooltip_widget.h"

class QnSliderTooltipWidget : public QnStyledTooltipWidget
{
    Q_OBJECT
    typedef QnStyledTooltipWidget base_type;

public:
    // Default margin between the tooltip and the slider.
    static constexpr qreal kToolTipMargin = 4.0;

    QnSliderTooltipWidget(QGraphicsItem* parent = nullptr);
};
