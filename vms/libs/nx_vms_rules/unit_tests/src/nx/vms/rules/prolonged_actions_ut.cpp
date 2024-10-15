// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/rules/rule.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_fields/target_devices_field.h>
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

static const auto resourceA = nx::Uuid("00000000-0000-0000-0001-00000000000A");
static const auto resourceB = nx::Uuid("00000000-0000-0000-0001-00000000000B");
static const auto resourceC = nx::Uuid("00000000-0000-0000-0001-00000000000C");

// Tests for true prolonged actions without duration tied to prolonged events.
class ProlongedActionsTest: public EngineBasedTest
{
protected:
    virtual void SetUp() override
    {
        plugin = std::make_unique<TestPlugin>(engine.get());

        engine->addEventConnector(&connector);
        engine->addActionExecutor(utils::type<TestProlongedAction>(), &executor);
    }

    virtual void TearDown() override
    {
        plugin.reset();
    }

    void givenRule(
        const UuidSelection& source,
        const UuidSelection& target,
        nx::Uuid id = nx::Uuid::createUuid())
    {
        auto rule = std::make_unique<Rule>(id, engine.get());

        auto filter = engine->buildEventFilter(utils::type<TestEventProlonged>());
        ASSERT_TRUE(filter);

        auto sourceField = filter->fieldByType<SourceCameraField>();
        ASSERT_TRUE(sourceField);
        sourceField->setAcceptAll(source.all);
        sourceField->setIds(source.ids);

        rule->addEventFilter(std::move(filter));

        auto builder = engine->buildActionBuilder(utils::type<TestProlongedAction>());
        ASSERT_TRUE(builder);

        auto devicesField = builder->fieldByType<TargetDevicesField>();
        ASSERT_TRUE(devicesField);
        devicesField->setUseSource(target.all);
        devicesField->setIds(target.ids);

        rule->addActionBuilder(std::move(builder));

        engine->updateRule(serialize(rule.get()));
    }

    void givenRuleABtoC()
    {
        givenRule({.ids = {resourceA, resourceB}}, {.ids = {resourceC}});
    }

    void whenTestEventFired(State state, nx::Uuid cameraId = resourceA)
    {
        auto event = TestEventProlongedPtr::create();
        event->setState(state);
        event->m_cameraId = cameraId;

        connector.process(event);
    }

    void thenActionExecuted(size_t index, State state, UuidSet resourceIds = {})
    {
        SCOPED_TRACE(NX_FMT("Expecting action at index: %1, state: %2, resources: %3",
            index, state, resourceIds).toStdString());

        ASSERT_GT(executor.actions.size(), index);

        const auto action = this->action(index);
        ASSERT_TRUE(action);

        EXPECT_EQ(action->type(), utils::type<TestProlongedAction>());
        EXPECT_EQ(action->state(), state);

        if (!resourceIds.empty())
            EXPECT_EQ(nx::utils::toQSet(action->m_deviceIds), resourceIds);
    }

    void thenTotalActionsExecuted(size_t size)
    {
        ASSERT_EQ(executor.actions.size(), size)
            << NX_FMT("Expected %1 total actions", size).toStdString();
    }

    QSharedPointer<TestProlongedAction> action(size_t index)
    {
        return executor.actions[index].staticCast<TestProlongedAction>();
    }

    QSharedPointer<TestProlongedAction> lastAction()
    {
        return executor.actions.back().staticCast<TestProlongedAction>();
    }

    std::unique_ptr<Plugin> plugin;
    TestEventConnector connector;
    TestActionExecutor executor;
};

TEST_F(ProlongedActionsTest, sanity)
{
    givenRuleABtoC();

    whenTestEventFired(State::started);
    thenActionExecuted(0, State::started);
    ASSERT_EQ(lastAction()->m_deviceIds, UuidList{resourceC});

    whenTestEventFired(State::stopped);
    thenActionExecuted(1, State::stopped);
}

TEST_F(ProlongedActionsTest, ignoreRepeatedStart)
{
    givenRuleABtoC();

    whenTestEventFired(State::started);
    thenActionExecuted(0, State::started);

    whenTestEventFired(State::started);
    thenTotalActionsExecuted(1);

    whenTestEventFired(State::started);
    thenTotalActionsExecuted(1);
}

TEST_F(ProlongedActionsTest, ignoreStopWithoutStart)
{
    givenRuleABtoC();

    whenTestEventFired(State::stopped);
    thenTotalActionsExecuted(0);
}

TEST_F(ProlongedActionsTest, startByDifferentResources)
{
    givenRuleABtoC();

    whenTestEventFired(State::started, resourceA);
    thenActionExecuted(0, State::started, {resourceC});
    thenTotalActionsExecuted(1);

    whenTestEventFired(State::started, resourceB);
    thenTotalActionsExecuted(1);
    // The action should not be started twice on the same resource.
}

TEST_F(ProlongedActionsTest, stopByDifferentResources)
{
    givenRuleABtoC();

    whenTestEventFired(State::started, resourceA);
    thenActionExecuted(0, State::started, {resourceC});
    thenTotalActionsExecuted(1);

    whenTestEventFired(State::started, resourceB);
    thenTotalActionsExecuted(1);

    whenTestEventFired(State::stopped, resourceA);
    thenTotalActionsExecuted(1);

    whenTestEventFired(State::stopped, resourceB);
    thenActionExecuted(1, State::stopped, {resourceC});
    thenTotalActionsExecuted(2);
    // The action should be stopped after all events are stopped.
}

TEST_F(ProlongedActionsTest, stopActionsIsEmittedWhenRuleIsDisabled)
{
    const auto ruleId = nx::Uuid::createUuid();
    // Action for B + C + source.
    givenRule({.all = true}, {.ids = {resourceB, resourceC}, .all = true}, ruleId);

    whenTestEventFired(State::started, resourceA);
    thenTotalActionsExecuted(1);

    // Disable rule.
    auto rule = engine->cloneRule(ruleId);
    rule->setEnabled(false);
    engine->updateRule(serialize(rule.get()));

    // And expects that auto generated stopped action contains all the required resources.
    thenTotalActionsExecuted(2);
    thenActionExecuted(1, State::stopped, {resourceA, resourceB, resourceC});
}

TEST_F(ProlongedActionsTest, startEventAfterRuleUpdate)
{
    const auto ruleId = nx::Uuid::createUuid();
    givenRule({.all = true}, {.ids = {resourceB}}, ruleId);

    whenTestEventFired(State::started, resourceA);
    thenTotalActionsExecuted(1);

    // Update rule, action is stopped.
    auto rule = engine->cloneRule(ruleId);
    rule->actionBuilders().front()->fieldByType<TargetDevicesField>()->setIds({resourceC});
    engine->updateRule(serialize(rule.get()));

    thenActionExecuted(1, State::stopped, {resourceB});

    // Rule state will reset, so start event will produce new action.
    whenTestEventFired(State::started, resourceA);
    thenActionExecuted(2, State::started, {resourceC});
}

} // nx::vms::rules::test
