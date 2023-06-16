// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_event_search_list_model.h"

#include <ui/workbench/workbench_access_controller.h>

#include "private/vms_event_search_list_model_p.h"

namespace nx::vms::client::desktop {

VmsEventSearchListModel::VmsEventSearchListModel(QnWorkbenchContext* context, QObject* parent):
    base_type(context, [this]() { return new Private(this); }, parent),
    d(qobject_cast<Private*>(base_type::d.data()))
{
    setLiveSupported(true);
    setLivePaused(true);
}

QString VmsEventSearchListModel::selectedEventType() const
{
    return d->selectedEventType();
}

void VmsEventSearchListModel::setSelectedEventType(const QString& value)
{
    d->setSelectedEventType(value);
}

QString VmsEventSearchListModel::selectedSubType() const
{
    return d->selectedSubType();
}

void VmsEventSearchListModel::setSelectedSubType(const QString& value)
{
    d->setSelectedSubType(value);
}

const QStringList& VmsEventSearchListModel::defaultEventTypes() const
{
    return d->defaultEventTypes();
}

void VmsEventSearchListModel::setDefaultEventTypes(const QStringList& value)
{
    d->setDefaultEventTypes(value);
}

bool VmsEventSearchListModel::isConstrained() const
{
    return !selectedEventType().isEmpty() || base_type::isConstrained();
}

bool VmsEventSearchListModel::hasAccessRights() const
{
    return accessController()->hasGlobalPermission(GlobalPermission::viewLogs);
}

} // namespace nx::vms::client::desktop
