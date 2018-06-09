#include "timeline_screenshot_cursor.h"

#include <ui/graphics/items/controls/time_slider.h>
#include <nx/client/desktop/timeline/graphics/timeline_cursor_layout.h>
#include <utils/common/event_processors.h>

namespace nx {
namespace client {
namespace desktop {

TimelineScreenshotCursor::TimelineScreenshotCursor(QnTimeSlider* slider, QGraphicsItem* tooltipParent):
    base_type(slider),
    m_slider(slider),
    m_layout(new TimelineCursorLayout()),
    m_mark(new QGraphicsLineItem(this))
{
    setParentItem(tooltipParent);
    setLayout(m_layout);
    setContentsMargins(QMarginsF());
    m_layout->setTimeContent(false, 0, true);
    m_mark->setParentItem(slider);
    m_mark->setPen(palette().mid().color());

    // we should set color for the mark on the timeline.
    installEventHandler(this, QEvent::PaletteChange, this,
        [this](QObject*, QEvent*) { m_mark->setPen(palette().mid().color()); });
    hide();
}

TimelineScreenshotCursor::~TimelineScreenshotCursor()
{
}

void TimelineScreenshotCursor::showAt(qreal position)
{
    QPointF point = mapToParent(mapFromItem(m_slider, position, -kToolTipMargin));
    pointTo(point);
    m_mark->setLine(position, 0, position, m_slider->geometry().height());
    show();
    qDebug() << "ScreenShot at" << position << point;
}

} // namespace desktop
} // namespace client
} // namespace nx
