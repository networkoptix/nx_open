#include "checked_button.h"

#include <array>
#include <QtCore/QEvent>
#include <QtGui/QPainter>

#include "utils/math/color_transformations.h"
#include "utils/math/linear_combination.h"
#include <nx/client/core/utils/geometry.h>

using nx::client::core::Geometry;

namespace {
    int lineWidth = 4;
}

QPixmap QnCheckedButton::generatePixmap(int size, const QColor &color, const QColor &insideColor, bool hovered, bool checked, bool enabled)
{
    QPixmap result(size, size);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);

    if (checked || hovered)
    {
        QColor penColor = palette().color(isEnabled() ? QPalette::Active : QPalette::Inactive, QPalette::Highlight);
        painter.setPen(QPen(penColor, lineWidth, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter.drawRect(Geometry::eroded(QRectF(result.rect()), lineWidth / 2.0));
    }

    QColor brushColor(color);
    if (!enabled)
        brushColor = toGrayscale(linearCombine(0.5, brushColor, 0.5, palette().color(QPalette::Disabled, QPalette::Background)));

    QRect rect = Geometry::eroded(result.rect(), lineWidth * 3 / 2);
    painter.fillRect(rect, brushColor);

    if (insideColor.toRgb() != color.toRgb())
    {
        brushColor = insideColor;
        if (!enabled)
            brushColor = toGrayscale(linearCombine(0.5, brushColor, 0.5, palette().color(QPalette::Disabled, QPalette::Background)));

        painter.setBrush(brushColor);
        painter.setPen(QPen(Qt::black, 1));

        qreal cellSize = result.width();
        qreal trOffset = cellSize / 6.4 + 0.5;
        qreal trSize = cellSize / 64.0 * 6.0 - 0.5;

        // TODO: #vkutin #common Code duplication with QnScheduleGridWidget
        std::array<QPointF, 4> points({
            QPointF(cellSize - trOffset - trSize, trOffset),
            QPointF(cellSize - trOffset, trOffset + trSize),
            QPointF(trOffset + trSize, cellSize - trOffset),
            QPointF(trOffset, cellSize - trSize - trOffset) });

        rect.adjust(0, 0, 1, 1);
        painter.drawLine(rect.bottomLeft(), rect.topRight());
        painter.drawPolygon(points.data(), static_cast<int>(points.size()));
    }

    painter.end();

    return result;
}

QnCheckedButton::QnCheckedButton(QWidget* parent):
    QToolButton(parent)
{
    updateIcon();
}

void QnCheckedButton::updateIcon() {
    const int size = 64;
    QColor insideColor = m_insideColor.isValid() ? m_insideColor : m_color;

    QPixmap normal          = generatePixmap(size, m_color, insideColor, false,  false, true);
    QPixmap normalDisabled  = generatePixmap(size, m_color, insideColor, false,  false, false);
    QPixmap normalHovered   = generatePixmap(size, m_color, insideColor, true,   false, true);
    QPixmap checked         = normalHovered;
    QPixmap checkedDisabled = generatePixmap(size, m_color, insideColor, false,  true, false);

    QIcon icon;

    icon.addPixmap(normal, QIcon::Normal, QIcon::Off);
    icon.addPixmap(normalDisabled, QIcon::Disabled, QIcon::Off);
    icon.addPixmap(normalHovered, QIcon::Active, QIcon::Off);
    icon.addPixmap(normal, QIcon::Selected, QIcon::Off);

    icon.addPixmap(checked, QIcon::Normal, QIcon::On);
    icon.addPixmap(checkedDisabled, QIcon::Disabled, QIcon::On);
    icon.addPixmap(checked, QIcon::Selected, QIcon::On);

    setIcon(icon);
}

QColor QnCheckedButton::color() const
{
    return m_color;
}

void QnCheckedButton::setColor(const QColor& color)
{
    if(m_color == color)
        return;

    m_color = color;

    updateIcon();
}

void QnCheckedButton::setInsideColor(const QColor& color)
{
    if(m_insideColor == color)
        return;

    m_insideColor = color;

    updateIcon();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool QnCheckedButton::event(QEvent *event) {
    if(event->type() == QEvent::EnabledChange)
        updateIcon();

    return base_type::event(event);
}

