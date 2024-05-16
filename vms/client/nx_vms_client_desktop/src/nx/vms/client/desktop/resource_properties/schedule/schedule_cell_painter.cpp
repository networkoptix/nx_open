// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "schedule_cell_painter.h"

#include <QtCore/QVariant>
#include <QtGui/QPainter>

#include <utils/math/color_transformations.h>
#include <nx/vms/client/core/skin/color_theme.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::client::core;

struct ScheduleCellPainter::Private
{
    ScheduleCellPainter* const q;

    const QColor emptyCellColor = core::colorTheme()->color("dark5");
    const QColor emptyCellHoveredColor = core::colorTheme()->color("dark6");
    const QColor nonEmptyCellColor = core::colorTheme()->color("green_core");
    const QColor nonEmptyCellHoveredColor = core::colorTheme()->color("green_l1");
    const QColor selectionFrameColor = core::colorTheme()->color("blue13");
};

ScheduleCellPainter::ScheduleCellPainter():
    d(new Private({this}))
{
}

ScheduleCellPainter::~ScheduleCellPainter()
{
}

void ScheduleCellPainter::paintCellBackground(
    QPainter* painter,
    const QRectF& rect,
    bool hovered,
    const QVariant& cellData) const
{
    if (cellData.toBool())
        painter->fillRect(rect, hovered ? d->nonEmptyCellHoveredColor : d->nonEmptyCellColor);
    else
        painter->fillRect(rect, hovered ? d->emptyCellHoveredColor : d->emptyCellColor);
}

void ScheduleCellPainter::paintSelectionFrame(QPainter* painter, const QRectF& rect) const
{
    static const QColor kSelectionBorderDelta(-48, -48, -48, 0);
    static const QColor kSelectionOpacityDelta(0, 0, 0, 0x80);

    painter->setPen(subColor(d->selectionFrameColor, kSelectionBorderDelta));
    painter->setBrush(subColor(d->selectionFrameColor, kSelectionOpacityDelta));
    painter->drawRect(rect);
}

void ScheduleCellPainter::paintCellText(QPainter*, const QRectF&, const QVariant&) const
{
}

} // namespace nx::vms::client::desktop
