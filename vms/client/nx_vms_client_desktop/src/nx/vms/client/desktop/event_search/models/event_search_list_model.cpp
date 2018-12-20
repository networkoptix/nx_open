#include "event_search_list_model.h"
#include "private/event_search_list_model_p.h"

#include <ui/workbench/workbench_access_controller.h>

namespace nx::vms::client::desktop {

EventSearchListModel::EventSearchListModel(QnWorkbenchContext* context, QObject* parent):
    base_type(context, [this]() { return new Private(this); }, parent),
    d(qobject_cast<Private*>(base_type::d.data()))
{
    setLiveSupported(true);
    setLivePaused(true);
}

vms::api::EventType EventSearchListModel::selectedEventType() const
{
    return d->selectedEventType();
}

void EventSearchListModel::setSelectedEventType(vms::api::EventType value)
{
    d->setSelectedEventType(value);
}

QString EventSearchListModel::selectedSubType() const
{
    return d->selectedSubType();
}

void EventSearchListModel::setSelectedSubType(const QString& value)
{
    d->setSelectedSubType(value);
}

bool EventSearchListModel::isConstrained() const
{
    return selectedEventType() != vms::api::undefinedEvent
        || base_type::isConstrained();
}

bool EventSearchListModel::hasAccessRights() const
{
    return accessController()->hasGlobalPermission(GlobalPermission::viewLogs);
}

} // namespace nx::vms::client::desktop
