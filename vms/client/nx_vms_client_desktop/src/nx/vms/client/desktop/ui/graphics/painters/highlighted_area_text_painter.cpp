// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "highlighted_area_text_painter.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>

#include <QtWidgets/QApplication>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kSpacing = 8;
static constexpr int kAdditionalTitleSpacing = 4;

using Line = QPair<QString, QString>;

QVector<Line> splitLines(const QString& text)
{
    QVector<Line> result;

    const auto lines = QStringView(text).split('\n');
    for (const auto& line: lines)
    {
        const auto tabIndex = line.indexOf('\t');

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

HighlightedAreaTextPainter::Fonts HighlightedAreaTextPainter::fonts() const
{
    return m_fonts;
}

void HighlightedAreaTextPainter::setFonts(const Fonts& fonts)
{
    m_fonts = fonts;
}

QColor HighlightedAreaTextPainter::color() const
{
    return m_color;
}

void HighlightedAreaTextPainter::setColor(const QColor& color)
{
    m_color = color;
}

QPixmap HighlightedAreaTextPainter::paintText(const QString& text, qreal devicePixelRatio) const
{
    if (text.isEmpty())
        return QPixmap();

    auto lines = splitLines(text);

    const bool hasTitle = lines.size() > 0
        && !lines[0].first.isEmpty()
        && lines[0].second.isEmpty();

    const auto title = hasTitle
        ? lines[0].first
        : QString();

    if (hasTitle)
        lines.removeFirst();

    QFontMetrics nameFontMetrics(m_fonts.name);
    QFontMetrics valueFontMetrics(m_fonts.value);
    QFontMetrics titleFontMetrics(m_fonts.title);

    const int titleWidth = hasTitle
        ? titleFontMetrics.horizontalAdvance(title)
        : 0;

    int maxNameWidth = 0;
    int maxValueWidth = 0;

    for (const auto& line: lines)
    {
        if (!line.first.isEmpty())
            maxNameWidth = qMax(maxNameWidth, nameFontMetrics.horizontalAdvance(line.first));
        if (!line.second.isEmpty())
            maxValueWidth = qMax(maxValueWidth, valueFontMetrics.horizontalAdvance(line.second));
    }

    const int nameLineSpacing = nameFontMetrics.lineSpacing();
    const int valueLineSpacing = valueFontMetrics.lineSpacing();
    const int lineSpacing =
        std::max(nameLineSpacing, valueLineSpacing);
    const int titleSpacing = hasTitle
        ? titleFontMetrics.lineSpacing() + kAdditionalTitleSpacing
        : 0;

    const auto width = qMax(maxNameWidth + maxValueWidth + kSpacing, titleWidth);
    const auto height = titleSpacing + lineSpacing * lines.size();
    const auto baseSize = QSize(width, height);
    const auto pixmapSize = baseSize * devicePixelRatio;

    QPixmap pixmap(pixmapSize);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setPen(m_color);
    painter.setRenderHint(QPainter::TextAntialiasing);

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

    if (hasTitle)
    {
        painter.setFont(m_fonts.title);
        painter.drawText(0, titleFontMetrics.ascent(), title);
        y += titleSpacing;
    }

    for (const auto& line: lines)
    {
        painter.setFont(m_fonts.name);
        painter.drawText(0, y + nameYShift, line.first);
        painter.setFont(m_fonts.value);
        painter.drawText(valueX, y + valueYShift, line.second);
        y += lineSpacing;
    }

    return pixmap;
}

} // namespace nx::vms::client::desktop
