#include "highlighted_area_text_painter.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>

namespace nx::vms::client::desktop {

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

QFont HighlightedAreaTextPainter::nameFont() const
{
    return m_nameFont;
}

void HighlightedAreaTextPainter::setNameFont(const QFont& font)
{
    m_nameFont = font;
}

QFont HighlightedAreaTextPainter::valueFont() const
{
    return m_valueFont;
}

void HighlightedAreaTextPainter::setValueFont(const QFont& font)
{
    m_valueFont = font;
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
    if (text.isEmpty())
        return QPixmap();

    const auto lines = splitLines(text);

    QFontMetrics nameFontMetrics(m_nameFont);
    QFontMetrics valueFontMetrics(m_valueFont);

    int maxNameWidth = 0;
    int maxValueWidth = 0;

    for (const auto& line: lines)
    {
        if (!line.first.isEmpty())
            maxNameWidth = qMax(maxNameWidth, nameFontMetrics.width(line.first));
        if (!line.second.isEmpty())
            maxValueWidth = qMax(maxValueWidth, valueFontMetrics.width(line.second));
    }

    const int nameLineSpacing = nameFontMetrics.lineSpacing();
    const int valueLineSpacing = valueFontMetrics.lineSpacing();
    const int lineSpacing =
        std::max(nameLineSpacing, valueLineSpacing);

    QPixmap pixmap(
        maxNameWidth + maxValueWidth + kSpacing,
        lineSpacing * lines.size());
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setPen(m_color);

    int nameYShift = 0;
    int valueYShift = (nameLineSpacing - valueLineSpacing) / 2;
    if (valueYShift < 0)
    {
        nameYShift = -valueYShift;
        valueYShift = 0;
    }
    nameYShift += nameFontMetrics.ascent();
    valueYShift += valueFontMetrics.ascent();

    int valueX = maxNameWidth + kSpacing;
    int y = 0;
    for (const auto& line: lines)
    {
        painter.setFont(m_nameFont);
        painter.drawText(0, y + nameYShift, line.first);
        painter.setFont(m_valueFont);
        painter.drawText(valueX, y + valueYShift, line.second);
        y += lineSpacing;
    }

    return pixmap;
}

} // namespace nx::vms::client::desktop
