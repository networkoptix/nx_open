// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/vms/api/rules/rule.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_fields/target_device_field.h>
#include <nx/vms/rules/action_executor.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_connector.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils/api.h>
#include <utils/common/synctime.h>

#include "mock_engine_events.h"
#include "test_infra.h"
#include "test_plugin.h"
#include "test_router.h"

namespace nx::vms::rules::test {

static const auto resourceA = QnUuid("00000000-0000-0000-0001-00000000000A");
static const auto resourceB = QnUuid("00000000-0000-0000-0001-00000000000B");
static const auto resourceC = QnUuid("00000000-0000-0000-0001-00000000000C");

class ProlongedActionsTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        engine = std::make_unique<Engine>(std::make_unique<TestRouter>());
        plugin = std::make_unique<TestPlugin>(engine.get());

        engine->addEventConnector(&connector);
        engine->addActionExecutor(utils::type<TestProlongedAction>(), &executor);

        auto rule = std::make_unique<Rule>(QnUuid::createUuid(), engine.get());
        rule->setEnabled(true);

        auto filter = engine->buildEventFilter(utils::type<TestEvent>());
        ASSERT_TRUE(filter);
        rule->addEventFilter(std::move(filter));

        auto builder = engine->buildActionBuilder(utils::type<TestProlongedAction>());
        ASSERT_TRUE(builder);

        auto devicesField = dynamic_cast<TargetDeviceField*>(
            builder->fields().value(utils::kDeviceIdsFieldName));
        ASSERT_TRUE(devicesField);
        devicesField->setIds({resourceC});

        rule->addActionBuilder(std::move(builder));

        engine->updateRule(serialize(rule.get()));
    }

    virtual void TearDown() override
    {
        plugin.reset();
        engine.reset();
    }

    void whenTestEventFired(State state, QnUuid cameraId = resourceA)
    {
        auto startEvent = QSharedPointer<TestEvent>::create();
        startEvent->setState(state);
        startEvent->m_cameraId = cameraId;

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

    void thenTotalActionsExecuted(size_t size)
    {
        ASSERT_EQ(executor.actions.size(), size);
    }

    QSharedPointer<TestProlongedAction> lastAction()
    {
        return executor.actions.back().staticCast<TestProlongedAction>();
    }

    void thenActionExecuted(size_t index, State state, QnUuid resourceId)
    {
        thenProlongedActionExecuted(index, state);
        EXPECT_EQ(lastAction()->m_deviceIds, QnUuidList{resourceId});
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
    ASSERT_EQ(lastAction()->m_deviceIds, QnUuidList{resourceC});

    whenTestEventFired(State::stopped);
    thenProlongedActionExecuted(1, State::stopped);
}

TEST_F(ProlongedActionsTest, ignoreRepeatedStart)
{
    whenTestEventFired(State::started);
    thenProlongedActionExecuted(0, State::started);

    whenTestEventFired(State::started);
    thenTotalActionsExecuted(1);

    whenTestEventFired(State::started);
    thenTotalActionsExecuted(1);
}

TEST_F(ProlongedActionsTest, ignoreStopWithoutStart)
{
    whenTestEventFired(State::stopped);
    thenTotalActionsExecuted(0);
}

TEST_F(ProlongedActionsTest, startByDifferentResources)
{
    whenTestEventFired(State::started, resourceA);
    thenActionExecuted(0, State::started, resourceC);
    thenTotalActionsExecuted(1);

    whenTestEventFired(State::started, resourceB);
    thenTotalActionsExecuted(1);
    // The action should not be started twice on the same resource.
}

} // nx::vms::rules::test
