// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/system_context_aware.h>
#include <nx/vms/rules/router.h>

namespace nx::vms::api::rules { struct EventInfo; }

namespace nx::vms::rules {

class Ec2Router:
    public Router,
    public nx::vms::common::SystemContextAware
{
public:
    Ec2Router(nx::vms::common::SystemContext* context);
    virtual ~Ec2Router();

    virtual void init(const nx::Uuid& id) override;

    virtual void routeEvent(
        const EventData& eventData,
        const QSet<nx::Uuid>& triggeredRules,
        const QSet<nx::Uuid>& affectedResources) override;

private:
    void onEventReceived(const nx::vms::api::rules::EventInfo& eventInfo);

private:
    nx::Uuid m_id;
    bool m_connected = false;
};

} // namespace nx::vms::rules
