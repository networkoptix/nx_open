#include "slider_tooltip_widget.h"

#include <QtWidgets/QApplication>

QnSliderTooltipWidget::QnSliderTooltipWidget(QGraphicsItem* parent):
    base_type(parent)
{
    setRoundingRadius(0.0);
    setTailLength(6.0);
    setTailWidth(12.0);
    setFont(QApplication::font());
}
