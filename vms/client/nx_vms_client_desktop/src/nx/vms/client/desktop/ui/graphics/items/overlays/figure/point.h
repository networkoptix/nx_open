// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "open_shape_figure.h"

namespace nx::vms::client::desktop::figure {

class Point: public OpenShapeFigure
{
    using base_type = OpenShapeFigure;

public:
    Point(
        const Points::value_type& point,
        const QColor& color,
        const QRectF& targetRect = QRectF(),
        core::StandardRotation rotation = core::StandardRotation::rotate0);

    virtual QRectF visualRect(const QSizeF& scale) const override;
    virtual bool isValid() const override;
    virtual FigurePtr clone() const override;
};

} // namespace nx::vms::client::desktop::figure
