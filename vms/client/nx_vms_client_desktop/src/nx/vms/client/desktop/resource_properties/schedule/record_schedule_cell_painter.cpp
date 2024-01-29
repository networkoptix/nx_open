// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "record_schedule_cell_painter.h"

#include <QtCore/QVariant>
#include <QtGui/QPainter>

#include <nx/utils/log/log.h>
#include <nx/vms/api/types/resource_types.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/resource_properties/schedule/record_schedule_cell_data.h>
#include <utils/common/scoped_painter_rollback.h>
#include <nx/vms/client/desktop/common/utils/stream_quality_strings.h>

namespace {

static constexpr auto kInternalRectOffset = 1.0 / 6.0;
static constexpr auto kBorderSize = 0.1;
static constexpr std::array<QPointF, 4> kInternalRectPolygonPoints({
        QPointF(1.0 - kInternalRectOffset - kBorderSize, kInternalRectOffset),
        QPointF(1.0 - kInternalRectOffset, kInternalRectOffset + kBorderSize),
        QPointF(kInternalRectOffset + kBorderSize, 1.0 - kInternalRectOffset),
        QPointF(kInternalRectOffset, 1.0 - kBorderSize - kInternalRectOffset)});

} // namespace

namespace nx::vms::client::desktop {

using RecordingType = nx::vms::api::RecordingType;

using MetadataType = nx::vms::api::RecordingMetadataType;
using MetadataTypes = nx::vms::api::RecordingMetadataTypes;

struct RecordScheduleCellPainter::Private
{
    RecordScheduleCellPainter* const q;

    RecordScheduleCellPainter::DisplayOptions displayOptions;

    const QColor emptyCellColor = core::colorTheme()->color("dark5");
    const QColor emptyCellHoveredColor = core::colorTheme()->color("dark6");
    const QColor recordAlwaysColor = core::colorTheme()->color("green_core");
    const QColor recordAlwaysHoveredColor = core::colorTheme()->color("green_l1");
    const QColor recordMotionColor = core::colorTheme()->color("red_d1");
    const QColor recordMotionHoveredColor = core::colorTheme()->color("red_core");
    const QColor recordObjectsColor = core::colorTheme()->color("yellow_d1");
    const QColor recordObjectsHoveredColor = core::colorTheme()->color("yellow_core");
    const QColor recordMotionAndObjectsColor = core::colorTheme()->color("orange_l2");
    const QColor recordMotionAndObjectsHoveredColor = recordMotionAndObjectsColor.lighter(110);
    const QColor cellBorderColor = core::colorTheme()->color("dark4");

    // The following two colors are used to choose the color of a cell text depending on its type.
    // After the latest change, we always use the same color for text, but the code has been kept
    // intact in case we need to roll back that change or update the colors again.
    const QColor lightCellTextColor = core::colorTheme()->color("@dark4");
    const QColor darkCellTextColor = core::colorTheme()->color("@dark4");

    QColor cellColor(RecordingType recordingType, MetadataTypes metadataTypes, bool hovered) const
    {
        if (recordingType == RecordingType::always)
            return hovered ? recordAlwaysHoveredColor : recordAlwaysColor;

        if (recordingType == RecordingType::metadataOnly
            || recordingType == RecordingType::metadataAndLowQuality)
        {
            if (metadataTypes.testFlag(MetadataType::motion)
                && metadataTypes.testFlag(MetadataType::objects))
            {
                return hovered ? recordMotionAndObjectsHoveredColor : recordMotionAndObjectsColor;
            }

            if (metadataTypes.testFlag(MetadataType::motion))
                return hovered ? recordMotionHoveredColor : recordMotionColor;

            if (metadataTypes.testFlag(MetadataType::objects))
                return hovered ? recordObjectsHoveredColor : recordObjectsColor;
        }

        NX_WARNING(this, "Unexpected recording parameters: type %1, metadata %2",
            recordingType, metadataTypes);
        return hovered ? emptyCellHoveredColor : emptyCellColor;
    }

    QColor cellTextColor(RecordingType recordingType, MetadataTypes metadataTypes) const
    {
        if (recordingType == RecordingType::always || recordingType == RecordingType::never)
            return lightCellTextColor;

        if (metadataTypes.testFlag(MetadataType::objects))
            return darkCellTextColor;

        return lightCellTextColor;
    }
};

RecordScheduleCellPainter::RecordScheduleCellPainter(DisplayOptions options):
    base_type(),
    d(new Private{this, options})
{
}

RecordScheduleCellPainter::~RecordScheduleCellPainter()
{
}

void RecordScheduleCellPainter::setDisplayOptions(const DisplayOptions& options)
{
    d->displayOptions = options;
}

void RecordScheduleCellPainter::paintCellBackground(
    QPainter* painter,
    const QRectF& rect,
    bool hovered,
    const QVariant& data) const
{
    if (data.isNull())
    {
        base_type::paintCellBackground(painter, rect, hovered, {});
        return;
    }

    const auto cellData = data.value<RecordScheduleCellData>();
    if (cellData.recordingType == RecordingType::never)
    {
        base_type::paintCellBackground(painter, rect, hovered, {});
        return;
    }

    const auto fillColor = d->cellColor(cellData.recordingType, cellData.metadataTypes, hovered);
    painter->fillRect(rect, fillColor);

    if (cellData.recordingType == RecordingType::metadataAndLowQuality)
    {
        QnScopedPainterBrushRollback brushRollback(painter,
            d->cellColor(RecordingType::always, {}, hovered));
        QnScopedPainterPenRollback penRollback(painter, QPen(d->cellBorderColor, 0));
        QnScopedPainterTransformRollback transformRollback(painter);
        painter->translate(rect.topLeft());
        painter->scale(rect.width(), rect.height());
        painter->drawLine(0.0, 1.0, 1.0, 0.0);
        painter->drawPolygon(
            kInternalRectPolygonPoints.data(),
            static_cast<int>(kInternalRectPolygonPoints.size()));
    }
}

void RecordScheduleCellPainter::paintCellText(
    QPainter* painter,
    const QRectF& rect,
    const QVariant& data) const
{
    if (d->displayOptions == DisplayOptions() || data.isNull())
        return;

    RecordScheduleCellData cellData = data.value<RecordScheduleCellData>();
    if (cellData.recordingType == RecordingType::never)
        return;

    QnScopedPainterPenRollback penRollback(painter, QPen(d->cellTextColor(
        cellData.recordingType, cellData.metadataTypes)));

    if (d->displayOptions.testFlag(DisplayOption::ShowFps)
        && d->displayOptions.testFlag(DisplayOption::ShowQuality))
    {
        QFont font;
        const auto fontPixelSize = rect.height() / 3 + 1;

        font.setPixelSize(fontPixelSize);
        QFontMetricsF fontMetrics(font);

        QMarginsF margins(3, 3, 3, 3);
        margins.setBottom(margins.bottom() - fontMetrics.descent() + 1);
        margins.setTop(margins.top() - fontPixelSize + fontMetrics.capHeight() + 1);
        QRectF contentRect = rect.marginsRemoved(margins);

        QnScopedPainterFontRollback fontRollback(painter, font);

        auto streamQualityString = toShortDisplayString(cellData.streamQuality);
        if (!cellData.isAutoBitrate())
            streamQualityString.append('*');

        painter->drawText(contentRect, Qt::AlignLeft | Qt::AlignTop, QString::number(cellData.fps));
        painter->drawText(contentRect, Qt::AlignRight | Qt::AlignBottom, streamQualityString);

    }
    else if (d->displayOptions.testFlag(DisplayOption::ShowFps))
    {
        QFont font;
        font.setPixelSize(rect.height() * 45 / 100);
        QnScopedPainterFontRollback fontRollback(painter, font);
        painter->drawText(rect, Qt::AlignCenter, QString::number(cellData.fps));
    }
    else if (d->displayOptions.testFlag(DisplayOption::ShowQuality))
    {
        QFont font;
        font.setPixelSize(rect.height() * 45 / 100);

        auto streamQualityString = toShortDisplayString(cellData.streamQuality);
        if (!cellData.isAutoBitrate())
            streamQualityString.append('*');

        QnScopedPainterFontRollback fontRollback(painter, font);
        painter->drawText(rect, Qt::AlignCenter, streamQualityString);
    }
}

} // namespace nx::vms::client::desktop
