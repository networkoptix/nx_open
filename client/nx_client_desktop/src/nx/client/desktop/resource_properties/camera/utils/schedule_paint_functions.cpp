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
    m_inside[Qn::RT_Never] = m_cell[Qn::RT_Never] = recordNever;
    m_inside[Qn::RT_MotionOnly] = m_cell[Qn::RT_MotionOnly] = recordMotion;
    m_inside[Qn::RT_Always] = m_cell[Qn::RT_Always] = recordAlways;

    m_insideHovered[Qn::RT_Never] = m_cellHovered[Qn::RT_Never] = recordNeverHovered;
    m_insideHovered[Qn::RT_MotionOnly] = m_cellHovered[Qn::RT_MotionOnly] = recordMotionHovered;
    m_insideHovered[Qn::RT_Always] = m_cellHovered[Qn::RT_Always] = recordAlwaysHovered;

    m_cell[Qn::RT_MotionAndLowQuality] = recordMotion;
    m_inside[Qn::RT_MotionAndLowQuality] = recordAlways;

    m_cellHovered[Qn::RT_MotionAndLowQuality] = recordMotionHovered;
    m_insideHovered[Qn::RT_MotionAndLowQuality] = recordAlwaysHovered;
}

void SchedulePaintFunctions::paintCell(
    QPainter* painter,
    const QRectF& rect,
    bool hovered,
    Qn::RecordingType type) const
{
    QColor color((hovered ? m_cellHovered : m_cell)[type]);
    QColor colorInside((hovered ? m_insideHovered : m_inside)[type]);

    painter->fillRect(rect, color);

    if (colorInside.toRgb() != color.toRgb())
    {
        QnScopedPainterBrushRollback brushRollback(painter, colorInside);
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
    const QColor defaultColor = m_cell[kDefaultRecordingType];
    const QColor brushColor = subColor(defaultColor, qnGlobals->selectionOpacityDelta());

    painter->setPen(subColor(defaultColor, qnGlobals->selectionBorderDelta()));
    painter->setBrush(brushColor);
    painter->drawRect(rect);
}

} // namespace desktop
} // namespace client
} // namespace nx
