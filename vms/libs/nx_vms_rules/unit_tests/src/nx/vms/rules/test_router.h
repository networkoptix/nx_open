// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/router.h>

namespace nx::vms::rules::test {

/** Reroutes events back to local engine. */
class TestRouter: public nx::vms::rules::Router
{
public:
    virtual void routeEvent(
        const EventData& eventData,
        const QSet<QnUuid>& triggeredRules,
        const QSet<QnUuid>& affectedResources) override
    {
        for (const auto ruleId: triggeredRules)
        {
            emit eventReceived(ruleId, eventData);
        }
    }
};


/** May be used for test fixture inheritance. */
struct TestEngineHolder
{
    std::unique_ptr<Engine> engine;

    TestEngineHolder():
        engine(std::make_unique<Engine>(std::make_unique<TestRouter>()))
    {
    }
};

} // namespace nx::vms::rules::test
