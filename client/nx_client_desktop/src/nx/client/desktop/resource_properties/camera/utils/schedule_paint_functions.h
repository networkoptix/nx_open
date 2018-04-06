#pragma once

#include <array>

#include <QtGui/QColor>

#include <common/common_globals.h>

#include <nx/client/desktop/common/utils/custom_painted.h>

namespace nx {
namespace client {
namespace desktop {

struct SchedulePaintFunctions
{
    const QColor recordNever;
    const QColor recordNeverHovered;

    const QColor recordAlways;
    const QColor recordAlwaysHovered;

    const QColor recordMotion;
    const QColor recordMotionHovered;

    const QColor border;

    SchedulePaintFunctions();

    void paintCell(
        QPainter* painter,
        const QRectF& rect,
        bool hovered,
        Qn::RecordingType type) const;
    CustomPaintedBase::PaintFunction paintCellFunction(Qn::RecordingType type) const;

    void paintSelection(QPainter* painter, const QRectF& rect) const;

private:
    using TypeColors = std::array<QColor, Qn::RT_Count>;
    TypeColors m_cell;
    TypeColors m_cellHovered;
    TypeColors m_inside;
    TypeColors m_insideHovered;
};

} // namespace desktop
} // namespace client
} // namespace nx
