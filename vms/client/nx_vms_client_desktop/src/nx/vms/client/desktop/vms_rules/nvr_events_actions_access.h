// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/types/event_rule_types.h>

class QnResourcePool;

namespace nx::vms::client::desktop {

class NvrEventsActionsAccess
{
public:
    static QList<nx::vms::api::EventType> removeInacessibleNvrEvents(
        const QList<nx::vms::api::EventType>& events,
        QnResourcePool* resourcePool);

    static QList<nx::vms::api::ActionType> removeInacessibleNvrActions(
        const QList<nx::vms::api::ActionType>& actions,
        QnResourcePool* resourcePool);
};

} // namespace nx::vms::client::desktop
