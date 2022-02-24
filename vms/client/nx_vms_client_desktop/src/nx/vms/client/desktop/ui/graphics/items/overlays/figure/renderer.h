// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "types.h"

class QWidget;
class QPainter;

namespace nx::vms::client::desktop::figure {

/** Incapsulates drawing functionality for the figures. */
class Renderer
{
public:
    explicit Renderer(
        int lineWidth,
        bool fillClosedShapes);

    ~Renderer();

    void renderFigure(
        QPainter* painter,
        QWidget* widget,
        const FigurePtr& figure,
        const QSizeF& widgetSize);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::figure
