// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/test_support/test_context.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/router.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils/type.h>
#include <utils/common/synctime.h>

namespace nx::vms::rules::test {

/** Reroutes events back to local engine. */
class TestRouter: public nx::vms::rules::Router
{
public:
    virtual void init(const QnCommonMessageProcessor* /*processor*/) override {};

    virtual void routeEvent(
        const EventPtr& event,
        const std::vector<ConstRulePtr>& triggeredRules) override
    {
        emit eventReceived(event, triggeredRules);
    }

    virtual void routeAction(const ActionPtr& action, Flags /*flags*/) override
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

    template<class Event, class Action>
    std::unique_ptr<Rule> makeRule(
        const Engine::EventConstructor& eventConstructor = []{ return new Event; },
        const Engine::ActionConstructor& actionConstructor = []{ return new Action; })
    {
        const auto eventType = rules::utils::type<Event>();
        const auto actionType = rules::utils::type<Action>();

        if (!engine->eventDescriptor(eventType))
            engine->registerEvent(Event::manifest(), eventConstructor);

        if (!engine->actionDescriptor(actionType))
            engine->registerAction(Action::manifest(), actionConstructor);

        auto rule = std::make_unique<Rule>(nx::Uuid::createUuid(), engine.get());

        rule->addEventFilter(engine->buildEventFilter(eventType));
        rule->addActionBuilder(engine->buildActionBuilder(actionType));

        return rule;
    }
};

} // namespace nx::vms::rules::test
