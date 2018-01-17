#include "highlighted_area_text_painter.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr int kSpacing = 8;

using Line = QPair<QString, QString>;

QVector<Line> splitLines(const QString& text)
{
    QVector<Line> result;

    for (const auto& line: text.splitRef(L'\n'))
    {
        const auto tabIndex = line.indexOf(L'\t');

        const auto fieldName = line.left(tabIndex).toString();
        const auto fieldValue = (tabIndex >= 0)
            ? line.mid(tabIndex + 1).toString()
            : QString();

        result.append(Line(fieldName, fieldValue));
    }

    return result;
}

} // namespace

HighlightedAreaTextPainter::HighlightedAreaTextPainter()
{
}

QFont HighlightedAreaTextPainter::font() const
{
    return m_font;
}

void HighlightedAreaTextPainter::setFont(const QFont& font)
{
    m_font = font;
}

QColor HighlightedAreaTextPainter::color() const
{
    return m_color;
}

void HighlightedAreaTextPainter::setColor(const QColor& color)
{
    m_color = color;
}

QPixmap HighlightedAreaTextPainter::paintText(const QString& text) const
{
    const auto lines = splitLines(text);

    QFontMetrics fontMetrics(m_font);

    int maxNameWidth = 0;
    int maxValueWidth = 0;

    for (const auto& line: lines)
    {
        if (!line.first.isEmpty())
            maxNameWidth = qMax(maxNameWidth, fontMetrics.width(line.first));
        if (!line.second.isEmpty())
            maxValueWidth = qMax(maxValueWidth, fontMetrics.width(line.second));
    }

    const int lineSpacing = fontMetrics.lineSpacing();

    QPixmap pixmap(
        maxNameWidth + maxValueWidth + kSpacing,
        lineSpacing * lines.size());
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setPen(m_color);
    painter.setFont(m_font);

    int y = fontMetrics.ascent();
    int valueX = maxNameWidth + kSpacing;
    for (const auto& line: lines)
    {
        painter.drawText(0, y, line.first);
        painter.drawText(valueX, y, line.second);
        y += lineSpacing;
    }

    return pixmap;
}

} // namespace desktop
} // namespace client
} // namespace nx
