// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "custom_painted.h"

namespace nx::vms::client::desktop {

CustomPaintedBase::PaintFunction CustomPaintedBase::customPaintFunction() const
{
    return m_paintFunction;
}

void CustomPaintedBase::setCustomPaintFunction(PaintFunction function)
{
    m_paintFunction = function;
}

bool CustomPaintedBase::customPaint(
    QPainter* painter, const QStyleOption* option, const QWidget* widget)
{
    if (m_paintFunction)
        return m_paintFunction(painter, option, widget);

    return false;
}

} // namespace nx::vms::client::desktop
