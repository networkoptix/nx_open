// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_bandwidth_schedule_cell_painter.h"

#include <cstdint>

#include <QtCore/QVariant>
#include <QtGui/QPainter>
#include <QtGui/QColor>

#include <nx/vms/client/desktop/resource_properties/schedule/schedule_grid_widget.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>
#include <nx/vms/client/core/skin/color_theme.h>

namespace {

static constexpr int64_t kBitsInMegabit = 1000 * 1000;

int bytesPerSecondToMegabitsPerSecond(int64_t bytesPerSecond)
{
    return bytesPerSecond * CHAR_BIT / kBitsInMegabit;
}

} // namespace

namespace nx::vms::client::desktop {

struct BackupBandwidthScheduleCellPainter::Private
{
    BackupBandwidthScheduleCellPainter* const q;

    const QColor noBackupCellColor = core::colorTheme()->color("dark5");
    const QColor noBackupHoveredCellColor = core::colorTheme()->color("dark6");
    const QColor noLimitColor = core::colorTheme()->color("green");
    const QColor noLimitHoveredColor = core::colorTheme()->color("green_l");
    const QColor limitColor = core::colorTheme()->color("red");
    const QColor limitHoveredColor = core::colorTheme()->color("red_l");
    const QColor cellTextColor = core::colorTheme()->color("light4");

    QColor cellColor(const QVariant& cellData, bool hovered) const
    {
        if (cellData.isNull())
            return hovered ? noLimitHoveredColor : noLimitColor;

        const auto bandwidthLimit = cellData.value<int64_t>();

        if (bandwidthLimit == 0)
            return hovered ? noBackupHoveredCellColor : noBackupCellColor;

        return hovered ? limitHoveredColor : limitColor;
    }
};

BackupBandwidthScheduleCellPainter::BackupBandwidthScheduleCellPainter():
    base_type(),
    d(new Private{this})
{
}

BackupBandwidthScheduleCellPainter::~BackupBandwidthScheduleCellPainter()
{
}

void BackupBandwidthScheduleCellPainter::paintCellBackground(
    QPainter* painter,
    const QRectF& rect,
    bool hovered,
    const QVariant& cellData) const
{
    const auto fillColor = d->cellColor(cellData, hovered);
    painter->fillRect(rect, fillColor);
}

void BackupBandwidthScheduleCellPainter::paintCellText(
    QPainter* painter,
    const QRectF& rect,
    const QVariant& cellData) const
{
    if (cellData.isNull())
        return;
    int64_t bitrateBytesPerSecond = cellData.value<int64_t>();
    if (bitrateBytesPerSecond == 0)
        return;

    QFont font;
    font.setPixelSize(rect.height() * 45 / 100);
    QnScopedPainterFontRollback(painter, font);
    QnScopedPainterPenRollback penRollback(painter, QPen(d->cellTextColor));
    painter->drawText(rect, Qt::AlignCenter,
        QString::number(bytesPerSecondToMegabitsPerSecond(bitrateBytesPerSecond)));
}

} // namespace nx::vms::client::desktop
