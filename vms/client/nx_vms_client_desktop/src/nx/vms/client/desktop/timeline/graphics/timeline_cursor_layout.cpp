#include "timeline_cursor_layout.h"

#include <QtWidgets/QGraphicsLinearLayout>

#include <translation/datetime_formatter.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>

namespace nx::vms::client::desktop {

namespace {
// Minimal width of big datetime tooltips.
const qreal kDateTimeTooltipMinimalWidth = 128.0;
} // namespace

TimelineCursorLayout::TimelineCursorLayout():
    QGraphicsLinearLayout(Qt::Vertical),
    m_tooltipLine1(new GraphicsLabel()),
    m_tooltipLine2(new GraphicsLabel())
{
    setContentsMargins(5.0, 4.0, 5.0, 3.0);
    setSpacing(2.0);
    addItem(m_tooltipLine1);
    addItem(m_tooltipLine2);

    m_tooltipLine1->setAlignment(Qt::AlignCenter);
    m_tooltipLine2->setAlignment(Qt::AlignCenter);

    QFont font;
    font.setPixelSize(font.pixelSize() + 2);
    font.setBold(true);
    m_tooltipLine2->setFont(font);
}

TimelineCursorLayout::~TimelineCursorLayout()
{
}

void TimelineCursorLayout::setTimeContent(bool isLive, milliseconds pos, bool showDate, bool showHours)
{
    if (!parentLayoutItem())
        return; // Layout belongs to another universe.

    QString line1;
    QString line2;

    if (isLive)
    {
        line1 = tr("Live");
    }
    else
    {
        if (showDate)
        {
            QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(pos.count());
            line1 = datetime::toString(dateTime.date(), datetime::Format::dd_MMMM_yyyy);
            line2 = datetime::toString(dateTime.time());
        }
        else
        {
            const auto format = showHours ? datetime::Format::hh_mm_ss : datetime::Format::mm_ss;
            line1 = datetime::toString(pos.count(), format);
        }
    }

    m_tooltipLine1->setText(line1);

    if (line2.isEmpty())
    {
        m_tooltipLine2->setVisible(false);
        parentLayoutItem()->setMinimumWidth(0.0);
    }
    else
    {
        m_tooltipLine2->setText(line2);
        m_tooltipLine2->setVisible(true);

        // Big datetime tooltips shouldn't be narrower than some minimal value.
        parentLayoutItem()->setMinimumWidth(kDateTimeTooltipMinimalWidth);
    }
}

} // namespace nx::vms::client::desktop
