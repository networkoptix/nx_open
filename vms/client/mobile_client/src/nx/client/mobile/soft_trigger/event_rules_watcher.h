// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/mobile/system_context_aware.h>
#include <nx/vms/event/event_fwd.h>

namespace nx::client::mobile {

// Tries to manually gather event rules for mobile client from old servers
class EventRulesWatcher:
    public QObject,
    public nx::vms::client::mobile::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    EventRulesWatcher(nx::vms::client::mobile::SystemContext* context, QObject* parent = nullptr);

private:
    void handleRulesReset(const nx::vms::event::RuleList& rules);
};

} // namespace nx::client::mobile
