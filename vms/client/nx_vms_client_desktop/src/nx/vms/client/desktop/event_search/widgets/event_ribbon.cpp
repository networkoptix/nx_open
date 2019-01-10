#include "event_ribbon.h"
#include "private/event_ribbon_p.h"

#include <QtWidgets/QScrollBar>
#include <QtGui/QHoverEvent>

#include <nx/vms/client/desktop/event_search/widgets/event_tile.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>

namespace nx::vms::client::desktop {

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

bool EventRibbon::headersEnabled() const
{
    return d->headersEnabled();
}

void EventRibbon::setHeadersEnabled(bool value)
{
    d->setHeadersEnabled(value);
}

Qt::ScrollBarPolicy EventRibbon::scrollBarPolicy() const
{
    return d->scrollBarPolicy();
}

void EventRibbon::setScrollBarPolicy(Qt::ScrollBarPolicy value)
{
    d->setScrollBarPolicy(value);
}

void EventRibbon::setInsertionMode(UpdateMode updateMode, bool scrollDown)
{
    d->setInsertionMode(updateMode, scrollDown);
}

void EventRibbon::setRemovalMode(UpdateMode updateMode)
{
    d->setRemovalMode(updateMode);
}

std::chrono::microseconds EventRibbon::highlightedTimestamp() const
{
    return d->highlightedTimestamp();
}

void EventRibbon::setHighlightedTimestamp(std::chrono::microseconds value)
{
    d->setHighlightedTimestamp(value);
}

QSet<QnResourcePtr> EventRibbon::highlightedResources() const
{
    return d->highlightedResources();
}

void EventRibbon::setHighlightedResources(const QSet<QnResourcePtr>& value)
{
    d->setHighlightedResources(value);
}

void EventRibbon::setViewportMargins(int top, int bottom)
{
    d->setViewportMargins(top, bottom);
}

QWidget* EventRibbon::viewportHeader() const
{
    return d->viewportHeader();
}

void EventRibbon::setViewportHeader(QWidget* value)
{
    d->setViewportHeader(value);
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
        case QEvent::Leave:
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
        case QEvent::HoverMove:
            d->updateHover();
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

nx::utils::Interval<int> EventRibbon::visibleRange() const
{
    return d->visibleRange();
}

} // namespace nx::vms::client::desktop
