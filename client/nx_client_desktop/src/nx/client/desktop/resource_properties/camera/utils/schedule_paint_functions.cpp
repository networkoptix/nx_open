#include "schedule_paint_functions.h"

#include <ui/style/globals.h>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>
#include <nx/client/desktop/ui/common/color_theme.h>

namespace {

static constexpr auto kInternalRectOffset = 1.0 / 6.0;
static constexpr auto kBorderSize = 0.1;
static constexpr Qn::RecordingType kDefaultRecordingType = Qn::RT_Always;

static constexpr std::array<QPointF, 4> kInternalRectPolygonPoints(
    {
        QPointF(1.0 - kInternalRectOffset - kBorderSize, kInternalRectOffset),
        QPointF(1.0 - kInternalRectOffset, kInternalRectOffset + kBorderSize),
        QPointF(kInternalRectOffset + kBorderSize, 1.0 - kInternalRectOffset),
        QPointF(kInternalRectOffset, 1.0 - kBorderSize - kInternalRectOffset)
    });

} // namespace

namespace nx {
namespace client {
namespace desktop {

SchedulePaintFunctions::SchedulePaintFunctions():
    recordNever(colorTheme()->color("dark5")),
    recordNeverHovered(colorTheme()->color("dark6")),
    recordAlways(colorTheme()->color("green_core")),
    recordAlwaysHovered(colorTheme()->color("green_l1")),
    recordMotion(colorTheme()->color("red_d1")),
    recordMotionHovered(colorTheme()->color("red_core")),
    border(colorTheme()->color("dark4"))
{
    m_cellColor[Qn::RT_Never][false] = recordNever;
    m_cellColor[Qn::RT_Never][true] = recordNeverHovered;

    m_cellColor[Qn::RT_MotionOnly][false] = recordMotion;
    m_cellColor[Qn::RT_MotionOnly][true] = recordMotionHovered;

    m_cellColor[Qn::RT_MotionAndLowQuality][false] = recordMotion;
    m_cellColor[Qn::RT_MotionAndLowQuality][true] = recordMotionHovered;

    m_cellColor[Qn::RT_Always][false] = recordAlways;
    m_cellColor[Qn::RT_Always][true] = recordAlwaysHovered;
}

void SchedulePaintFunctions::paintCell(
    QPainter* painter,
    const QRectF& rect,
    bool hovered,
    Qn::RecordingType type) const
{
    painter->fillRect(rect, m_cellColor[type][hovered]);

    if (type == Qn::RT_MotionAndLowQuality)
    {
        QnScopedPainterBrushRollback brushRollback(painter, m_cellColor[Qn::RT_Always][hovered]);
        QnScopedPainterPenRollback penRollback(painter, QPen(border, 0));
        QnScopedPainterTransformRollback transformRollback(painter);
        painter->translate(rect.topLeft());
        painter->scale(rect.width(), rect.height());
        painter->drawLine(0.0, 1.0, 1.0, 0.0);
        painter->drawPolygon(
            kInternalRectPolygonPoints.data(),
            static_cast<int>(kInternalRectPolygonPoints.size()));
    }
}

CustomPaintedBase::PaintFunction SchedulePaintFunctions::paintCellFunction(
    Qn::RecordingType type) const
{
    return
        [this, type](
        QPainter* painter,
        const QStyleOption* option,
        const QWidget* /*widget*/) -> bool
        {
            const auto hovered = option->state.testFlag(QStyle::State_MouseOver);
            paintCell(painter, option->rect, hovered, type);
            return true;
        };
}

void SchedulePaintFunctions::paintSelection(QPainter* painter, const QRectF& rect) const
{
    const QColor defaultColor = m_cellColor[kDefaultRecordingType][false];
    const QColor brushColor = subColor(defaultColor, qnGlobals->selectionOpacityDelta());

    painter->setPen(subColor(defaultColor, qnGlobals->selectionBorderDelta()));
    painter->setBrush(brushColor);
    painter->drawRect(rect);
}

} // namespace desktop
} // namespace client
} // namespace nx
