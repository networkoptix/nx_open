// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/router.h>
#include <nx/vms/rules/rule.h>
#include <utils/common/synctime.h>

namespace nx::vms::rules::test {

/** Reroutes events back to local engine. */
class TestRouter: public nx::vms::rules::Router
{
public:
    virtual void routeEvent(
        const EventPtr& event,
        const std::vector<ConstRulePtr>& triggeredRules) override
    {
        for (const auto& rule: triggeredRules)
        {
            emit eventReceived(event, triggeredRules);
        }
    }

    virtual void routeAction(const ActionPtr& action) override
    {
        receiveAction(action);
    }

    virtual nx::Uuid peerId() const override
    {
        static auto result = nx::Uuid::createUuid();
        return result;
    }
};

/** May be used for test fixture inheritance. */
struct TestEngineHolder
{
    QnSyncTime syncTime;
    std::unique_ptr<Engine> engine;

    explicit TestEngineHolder(nx::vms::common::SystemContext* context):
        engine(std::make_unique<Engine>(std::make_unique<TestRouter>()))
    {
        context->setVmsRulesEngine(engine.get());
    }
};

} // namespace nx::vms::rules::test
