// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/router.h>
#include <nx/vms/rules/rule.h>
#include <utils/common/synctime.h>

namespace nx::vms::rules::test {

/** Reroutes events back to local engine. */
class TestRouter: public nx::vms::rules::Router
{
public:
    virtual void init(const QnCommonMessageProcessor* processor) override {};

    virtual void routeEvent(
        const EventPtr& event,
        const std::vector<ConstRulePtr>& triggeredRules) override
    {
        emit eventReceived(event, triggeredRules);
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

struct EngineBasedTest: public common::test::ContextBasedTest
{
    QnSyncTime syncTime;
    std::unique_ptr<Engine> engine;

    EngineBasedTest():
        engine(std::make_unique<Engine>(
            systemContext(),
            std::make_unique<TestRouter>()))
    {
    }
};

} // namespace nx::vms::rules::test
