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
}

EventRibbon::~EventRibbon()
{
}

QAbstractListModel* EventRibbon::model() const
{
    return d->model();
}

void EventRibbon::setModel(QAbstractListModel* model)
{
    d->setModel(model);
}

QScrollBar* EventRibbon::scrollBar() const
{
    return d->scrollBar();
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

} // namespace desktop
} // namespace client
} // namespace nx
