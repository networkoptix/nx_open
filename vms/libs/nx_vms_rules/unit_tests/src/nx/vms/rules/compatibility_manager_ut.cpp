// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>

#include <gtest/gtest.h>

#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/utils/compatibility_manager.h>

#include "test_action.h"
#include "test_event.h"
#include "test_plugin.h"
#include "test_router.h"

namespace nx::vms::rules::test {

using namespace std::chrono_literals;

class PlainEvent: public nx::vms::rules::BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "testPlain")

public:
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<PlainEvent>(),
            .displayName = TranslatableString("Plain event"),
            .flags = {ItemFlag::instant}
        };
    }

    using BasicEvent::BasicEvent;

    QString resourceKey() const override
    {
        return {};
    }
};

class CompatibilityManagerTest: public EngineBasedTest, public TestPlugin
{
public:
    CompatibilityManagerTest(): TestPlugin{engine.get()}
    {
        registerEvent<PlainEvent>();
    }

    template<class Event, class Action>
    void givenRule()
    {
        m_rule = makeRule<Event, Action>();
        m_compatibilityManager =
            std::make_unique<utils::CompatibilityManager>(m_rule.get(), systemContext());
        m_compatibilityManager->setDefaultActionType(utils::type<TestAction>());
    }

    void withActionDuration(std::chrono::microseconds duration)
    {
        if (auto field = actionField<OptionalTimeField>(utils::kDurationFieldName); NX_ASSERT(field))
            field->setValue(duration);
    }

    void withSelectedDevicesForAction(const UuidSet& devices, bool useSource)
    {
        if (auto field = actionField<TargetDevicesField>(utils::kDeviceIdsFieldName); NX_ASSERT(field))
        {
            field->setIds(devices);
            field->setUseSource(useSource);
        }
    }

    void withSelectedDevicesForEvent(const UuidSet& devices)
    {
        if (auto field = eventField<SourceCameraField>(utils::kCameraIdFieldName); NX_ASSERT(field))
            field->setIds(devices);
    }

    void withState(State state)
    {
        if (auto field = eventField<StateField>(utils::kStateFieldName); NX_ASSERT(field))
            field->setValue(state);
    }

    void selectedEventDevicesAre(const UuidSet& devices)
    {
        if (auto field = eventField<SourceCameraField>(utils::kCameraIdFieldName); NX_ASSERT(field))
            EXPECT_EQ(field->ids(), devices);
    }

    void selectedActionDevicesAre(const UuidSet& devices, bool useSource)
    {
        if (auto field = actionField<TargetDevicesField>(utils::kDeviceIdsFieldName); NX_ASSERT(field))
        {
            EXPECT_EQ(field->ids(), devices);
            EXPECT_EQ(field->useSource(), useSource);
        }
    }

    template<class Action>
    void actionTypeIs()
    {
        EXPECT_EQ(utils::type<Action>(), m_rule->actionBuilders().first()->actionType());
    }

    void durationIsNonZero()
    {
        if (auto field = actionField<OptionalTimeField>(utils::kDurationFieldName); NX_ASSERT(field))
            EXPECT_NE(field->value(), std::chrono::microseconds::zero());
    }

    void stateIs(State state)
    {
        if (auto field = eventField<StateField>(utils::kStateFieldName); NX_ASSERT(field))
            EXPECT_EQ(field->value(), state);
    }

    template<class Event>
    void whenEventIsChangedTo()
    {
        m_compatibilityManager->changeEventType(utils::type<Event>());
    }

    template<class Action>
    void whenActionIsChangedTo()
    {
        m_compatibilityManager->changeActionType(utils::type<Action>());
    }

private:
    std::unique_ptr<Rule> m_rule;
    std::unique_ptr<utils::CompatibilityManager> m_compatibilityManager;

    template<class Field>
    Field* actionField(const char* name)
    {
        return m_rule->actionBuilders().first()->fieldByName<Field>(name);
    }

    template<class Field>
    Field* eventField(const char* name)
    {
        return m_rule->eventFilters().first()->fieldByName<Field>(name);
    }
};

TEST_F(CompatibilityManagerTest, durationForInstantEvent)
{
    // Given rule with the action duration of event.
    givenRule<TestEventProlonged, TestProlongedAction>();
    withState(State::none);
    withActionDuration(0s);

    // When the action is changed to instant.
    whenActionIsChangedTo<TestAction>();
    // State must be fixed from `none` to `started`.
    stateIs(State::started);
}

TEST_F(CompatibilityManagerTest, durationAdjustedProperly)
{
    // Given rule with the action duration of event.
    givenRule<TestEventProlonged, TestProlongedAction>();
    withState(State::none);
    withActionDuration(0s);

    // When event is changed to instant.
    whenEventIsChangedTo<TestEventInstant>();
    // Action type must not be changed.
    actionTypeIs<TestProlongedAction>();
    // Duration must be fixed to some non zero value.
    durationIsNonZero();
}

TEST_F(CompatibilityManagerTest, actionTypeChanged)
{
    // Given rule with the prolonged event and prolonged action without duration.
    givenRule<TestEventProlonged, TestProlongedOnlyAction>();
    // When event is changed to instant.
    whenEventIsChangedTo<PlainEvent>();

    // Prolonged action no more compatible with this event and must be fixed to some default one.
    actionTypeIs<TestAction>();
}

TEST_F(CompatibilityManagerTest, useSource)
{
    // Given rule with the event and action with device selection.
    givenRule<TestEventInstant, TestProlongedAction>();
    // Action uses `useSource` in its device selection.
    withSelectedDevicesForAction({}, /*useSource*/ true);

    // The new one event does not have camera.
    whenEventIsChangedTo<PlainEvent>();
    // `useSource` must be fixed to false.
    selectedActionDevicesAre({}, /*useSource*/ false);
}

TEST_F(CompatibilityManagerTest, selectedDevices)
{
    givenRule<TestEventInstant, TestProlongedAction>();

    const auto eventDevices = {Uuid::createUuid(), Uuid::createUuid(), Uuid::createUuid()};
    withSelectedDevicesForEvent(eventDevices);
    const auto actionDevices = {Uuid::createUuid(), Uuid::createUuid(), Uuid::createUuid()};
    withSelectedDevicesForAction(actionDevices, /*useSource*/ false);

    // Selected devices are moved to the new event.
    whenEventIsChangedTo<TestEventProlonged>();
    selectedEventDevicesAre(eventDevices);

    // Selected devices are moved to the new action.
    whenActionIsChangedTo<TestActionWithPermissions>();
    selectedActionDevicesAre(actionDevices, /*useSource*/ false);
}

} // namespace nx::vms::rules::test

#include "compatibility_manager_ut.moc"
