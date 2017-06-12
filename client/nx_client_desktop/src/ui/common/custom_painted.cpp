#include "custom_painted.h"

CustomPaintedBase::PaintFunction CustomPaintedBase::customPaintFunction() const
{
    return m_paintFunction;
}

void CustomPaintedBase::setCustomPaintFunction(PaintFunction function)
{
    m_paintFunction = function;
}

bool CustomPaintedBase::customPaint(QPainter* painter, const QStyleOption* option, const QWidget* widget)
{
    if (m_paintFunction)
        return m_paintFunction(painter, option, widget);

    return false;
}
