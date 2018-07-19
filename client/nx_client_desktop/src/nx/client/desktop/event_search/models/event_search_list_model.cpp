#include "event_search_list_model.h"
#include "private/event_search_list_model_p.h"

namespace nx {
namespace client {
namespace desktop {

EventSearchListModel::EventSearchListModel(QObject* parent):
    base_type([this]() { return new Private(this); }, parent),
    d(qobject_cast<Private*>(d_func()))
{
    setLive(true);
}

vms::api::EventType EventSearchListModel::selectedEventType() const
{
    return d->selectedEventType();
}

void EventSearchListModel::setSelectedEventType(vms::api::EventType value)
{
    d->setSelectedEventType(value);
}

bool EventSearchListModel::isConstrained() const
{
    return selectedEventType() != vms::api::undefinedEvent
        || base_type::isConstrained();
}

} // namespace desktop
} // namespace client
} // namespace nx
