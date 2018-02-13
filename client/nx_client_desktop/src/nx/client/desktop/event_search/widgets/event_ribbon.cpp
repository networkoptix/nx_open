#include "event_ribbon.h"
#include "private/event_ribbon_p.h"

#include <QtWidgets/QScrollBar>
#include <QtGui/QHoverEvent>

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

bool EventRibbon::showDefaultToolTips() const
{
    return d->showDefaultToolTips();
}

void EventRibbon::setShowDefaultToolTips(bool value)
{
    d->setShowDefaultToolTips(value);
}

bool EventRibbon::previewsEnabled() const
{
    return d->previewsEnabled();
}

void EventRibbon::setPreviewsEnabled(bool value)
{
    d->setPreviewsEnabled(value);
}

bool EventRibbon::footersEnabled() const
{
    return d->footersEnabled();
}

void EventRibbon::setFootersEnabled(bool value)
{
    d->setFootersEnabled(value);
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
    switch (event->type())
    {
        case QEvent::Wheel:
        {
            // TODO: #vkutin Implement smooth animated scroll.
            if (d->scrollBar()->isVisible())
                d->scrollBar()->event(event);

            event->accept();
            return true;
        }

        case QEvent::Enter:
            d->updateHover(true, static_cast<QEnterEvent*>(event)->pos());
            break;

        case QEvent::HoverEnter:
        case QEvent::HoverMove:
            d->updateHover(true, static_cast<QHoverEvent*>(event)->pos());
            break;

        case QEvent::Leave:
        case QEvent::HoverLeave:
            d->updateHover(false, QPoint());
            break;

        default:
            break;
    }

    return base_type::event(event);
}

int EventRibbon::count() const
{
    return d->count();
}

int EventRibbon::unreadCount() const
{
    return d->unreadCount();
}

} // namespace desktop
} // namespace client
} // namespace nx
