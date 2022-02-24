// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QtGlobal>

namespace nx::vms::client::desktop {

class GraphicsStyle
{
public:
    static qreal sliderPositionFromValue(
        qint64 min,
        qint64 max,
        qint64 logicalValue,
        qreal span,
        bool upsideDown,
        bool bound);

    static qint64 sliderValueFromPosition(
        qint64 min,
        qint64 max,
        qreal pos,
        qreal span,
        bool upsideDown,
        bool bound);
};

} // namespace nx::vms::client::desktop
