#include "event_ribbon.h"
#include "private/event_ribbon_p.h"

#include <QtWidgets/QScrollBar>

#include <nx/client/desktop/event_search/widgets/event_tile.h>

namespace nx {
namespace client {
namespace desktop {

EventRibbon::EventRibbon(QWidget* parent):
    base_type(parent, Qt::FramelessWindowHint),
    d(new Private(this))
{
    // TODO: #removeme #vkutin This is just for testing.
    setMinimumSize(0, 500);
}

EventRibbon::~EventRibbon()
{
}

void EventRibbon::insertTile(int index, EventTile* tile)
{
    d->insertTile(index, tile);
}

void EventRibbon::removeTiles(int first, int count)
{
    d->removeTiles(first, count);
}

QSize EventRibbon::sizeHint() const
{
    return QSize(-1, d->totalHeight());
}

bool EventRibbon::event(QEvent* event)
{
    if (event->type() != QEvent::Wheel)
        return base_type::event(event);

    // TODO: #vkutin Implement smooth animated scroll.

    if (d->scrollBar()->isVisible())
        d->scrollBar()->event(event);

    event->accept();
    return true;
}

} // namespace
} // namespace client
} // namespace nx
