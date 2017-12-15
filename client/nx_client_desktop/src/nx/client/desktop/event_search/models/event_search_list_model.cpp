#include "event_search_list_model.h"
#include "private/event_search_list_model_p.h"

namespace nx {
namespace client {
namespace desktop {

EventSearchListModel::EventSearchListModel(QObject* parent):
    base_type([this]() { return new Private(this); }, parent),
    d(qobject_cast<Private*>(d_func()))
{
}

vms::event::EventType EventSearchListModel::selectedEventType() const
{
    return d->selectedEventType();
}

void EventSearchListModel::setSelectedEventType(vms::event::EventType value)
{
    d->setSelectedEventType(value);
}

} // namespace
} // namespace client
} // namespace nx
