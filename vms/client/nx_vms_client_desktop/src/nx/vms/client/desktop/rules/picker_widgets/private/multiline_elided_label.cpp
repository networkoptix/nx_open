// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "multiline_elided_label.h"

#include <QtGui/QPainter>
#include <QtGui/QTextLayout>

namespace nx::vms::client::desktop::rules {

MultilineElidedLabel::MultilineElidedLabel(QWidget* parent): QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void MultilineElidedLabel::setText(const QString& text)
{
    const auto trimmedText = text.trimmed();
    if (m_text == trimmedText)
        return;

    m_text = trimmedText;
    update();
}

const QString& MultilineElidedLabel::text() const
{
    return m_text;
}

void MultilineElidedLabel::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);

    QTextOption textOption{Qt::AlignRight | Qt::AlignVCenter};
    textOption.setWrapMode(QTextOption::WrapMode::WrapAtWordBoundaryOrAnywhere);

    const auto fontMetrics = painter.fontMetrics();
    const auto widgetWidth = width();
    const auto lineSpacing = fontMetrics.lineSpacing();
    const auto isSingleWord = !m_text.contains(" ");
    const auto isMultiline = fontMetrics.boundingRect(m_text, textOption).width() > widgetWidth
        && !isSingleWord;
    int y = 0;

    QTextLayout textLayout(m_text, painter.font());
    textLayout.setTextOption(textOption);
    textLayout.beginLayout();

    for(;;)
    {
        QTextLine line = textLayout.createLine();
        if (!line.isValid())
            break;

        line.setLineWidth(widgetWidth);
        int nextLineY = y + lineSpacing;

        // Check whether the given line is the last one.
        if (height() >= nextLineY + lineSpacing)
        {
            if (!isMultiline)
            {
                // The given widget must have only one line vertically aligned to the center.

                const auto text = fontMetrics.boundingRect(m_text, textOption).width() > widgetWidth
                    ? fontMetrics.elidedText(m_text, Qt::ElideRight, widgetWidth)
                    : m_text;

                painter.drawText(rect(), text, Qt::AlignRight | Qt::AlignVCenter);

                setToolTip(text == m_text ? QString{} : m_text);
                break;
            }

            line.draw(&painter, QPoint(0, y));
            y = nextLineY;
        }
        else
        {
            // This is the last line. The text for witch it is not enough space must be elided.
            const auto lastLine = m_text.mid(line.textStart());
            const auto elidedLastLine =
                fontMetrics.elidedText(lastLine, Qt::ElideRight, widgetWidth);

            painter.drawText(QRect{0, y, widgetWidth, lineSpacing}, elidedLastLine, textOption);

            setToolTip(lastLine == elidedLastLine ? QString{} : m_text);
            break;
        }
    }

    textLayout.endLayout();
}

} // namespace nx::vms::client::desktop::rules
