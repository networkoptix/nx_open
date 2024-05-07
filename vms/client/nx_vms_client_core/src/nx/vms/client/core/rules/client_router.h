// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/system_context_aware.h>
#include <nx/vms/rules/router.h>

namespace nx::vms::client::core::rules {

class ClientRouter:
    public nx::vms::rules::Router,
    public nx::vms::client::core::SystemContextAware
{
public:
    explicit ClientRouter(nx::vms::client::core::SystemContext* context);
    virtual ~ClientRouter() override;

    virtual void init(const QnCommonMessageProcessor* processor) override;

    virtual nx::Uuid peerId() const override;

    virtual void routeEvent(
        const nx::vms::rules::EventPtr& event,
        const RuleList& triggeredRules) override;

    virtual void routeAction(const nx::vms::rules::ActionPtr& action) override;

private:
    void onActionReceived(const nx::vms::api::rules::ActionInfo& info);
};

} // namespace nx::vms::client::core::rules
