// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/vms/api/rules/rule.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_executor.h>
#include <nx/vms/rules/event_connector.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/rule.h>
#include <utils/common/synctime.h>

#include "mock_engine_events.h"
#include "test_plugin.h"
#include "test_router.h"

namespace nx::vms::rules::test {

class TestActionExecutor: public ActionExecutor
{
public:
    void execute(const ActionPtr& action)
    {
        actions.push_back(action);
    }

public:
    std::vector<ActionPtr> actions;
};

class TestEventConnector: public EventConnector
{
public:
    void process(const EventPtr& e)
    {
        emit event(e);
    }
};

class ProlongedActionsTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        engine = std::make_unique<Engine>(std::make_unique<TestRouter>());
        plugin = std::make_unique<TestPlugin>(engine.get());

        engine->addEventConnector(&connector);
        engine->addActionExecutor(utils::type<TestProlongedAction>(), &executor);

        auto rule = std::make_unique<Rule>(QnUuid::createUuid());
        rule->setEnabled(true);
        rule->addEventFilter(engine->buildEventFilter(utils::type<TestEvent>()));
        rule->addActionBuilder(engine->buildActionBuilder(utils::type<TestProlongedAction>()));

        engine->updateRule(std::move(rule));
    }

    virtual void TearDown() override
    {
        plugin.reset();
        engine.reset();
    }

    void whenTestEventFired(State state)
    {
        auto startEvent = QSharedPointer<TestEvent>::create();
        startEvent->setState(state);

        connector.process(startEvent);
    }

    void thenProlongedActionExecuted(size_t index, State state)
    {
        ASSERT_GT(executor.actions.size(), index);

        const auto action = executor.actions[index];
        ASSERT_TRUE(action);

        EXPECT_EQ(action->type(), utils::type<TestProlongedAction>());
        EXPECT_EQ(action->state(), state);
    }

    std::unique_ptr<Engine> engine;
    std::unique_ptr<Plugin> plugin;
    TestEventConnector connector;
    TestActionExecutor executor;

private:
    QnSyncTime syncTime;
};

TEST_F(ProlongedActionsTest, sanity)
{
    whenTestEventFired(State::started);
    thenProlongedActionExecuted(0, State::started);

    whenTestEventFired(State::stopped);
    thenProlongedActionExecuted(1, State::stopped);
}

} // nx::vms::rules::test
