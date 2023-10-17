// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_search_list_model.h"

#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/system_context.h>

#include "private/event_search_list_model_p.h"

namespace nx::vms::client::desktop {

EventSearchListModel::EventSearchListModel(WindowContext* context, QObject* parent):
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

std::vector<nx::vms::api::EventType> EventSearchListModel::defaultEventTypes() const
{
    return d->defaultEventTypes();
}

void EventSearchListModel::setDefaultEventTypes(const std::vector<nx::vms::api::EventType>& value)
{
    d->setDefaultEventTypes(value);
}

bool EventSearchListModel::isConstrained() const
{
    return selectedEventType() != vms::api::undefinedEvent
        || base_type::isConstrained();
}

bool EventSearchListModel::hasAccessRights() const
{
    return system()->accessController()->hasGlobalPermissions(GlobalPermission::viewLogs);
}

} // namespace nx::vms::client::desktop
