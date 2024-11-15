// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QtCore/QThread>

#include <nx/utils/qobject.h>
#include <nx/vms/api/rules/rule.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_fields/optional_time_field.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils/api.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

#include "mock_engine_events.h"
#include "test_action.h"
#include "test_event.h"
#include "test_field.h"
#include "test_infra.h"
#include "test_plugin.h"
#include "test_router.h"

namespace nx::vms::rules::test {

using namespace std::chrono;

namespace {

const QString kTestEventId = "testEvent";
const QString kTestActionId = "testAction";
const QString kTestEventFieldId = fieldMetatype<TestEventField>();
const QString kTestActionFieldId = fieldMetatype<TestActionField>();

const FieldDescriptor kTestEventFieldDescriptor{.id = kTestEventFieldId};
const FieldDescriptor kTestActionFieldDescriptor{.id = kTestActionFieldId};

const TranslatableString kDisplayName("Display Name");
const TranslatableString kEventName("Event Name");
const TranslatableString kActionName("Action Name");

api::Rule makeEmptyRuleData()
{
    return {{nx::Uuid::createUuid()}};
}

} // namespace

class EngineTest: public EngineBasedTest
{
protected:
    Engine::EventConstructor testEventConstructor = [] { return new TestEvent; };
    Engine::ActionConstructor testActionConstructor = [] { return new TestAction; };
    Engine::EventFieldConstructor testEventFieldConstructor =
        [](const FieldDescriptor* descriptor) { return new TestEventField{descriptor}; };
};

TEST_F(EngineTest, ruleAddedSuccessfully)
{
    // No rules at the moment the engine is just created.
    ASSERT_EQ(0, engine->rules().size());

    auto rule = std::make_unique<Rule>(nx::Uuid::createUuid(), engine.get());
    engine->updateRule(serialize(rule.get()));

    ASSERT_EQ(engine->rules().size(), 1);
    ASSERT_TRUE(engine->rule(rule->id()));
}

TEST_F(EngineTest, ruleClonedSuccessfully)
{
    auto rule = std::make_unique<Rule>(nx::Uuid::createUuid(), engine.get());
    engine->updateRule(serialize(rule.get()));

    auto clonedRule = engine->cloneRule(rule->id());

    ASSERT_TRUE(clonedRule);
    ASSERT_EQ(rule->id(), clonedRule->id());
}

TEST_F(EngineTest, engineCreatedWithoutEventsAndActions)
{
    // No actions and events are registered at the moment the engine is just created.
    ASSERT_TRUE(engine->events().empty());
    ASSERT_TRUE(engine->actions().empty());

    // An attempt to get a non-registered action or event descriptor must return nullopt.
    ASSERT_EQ(engine->eventDescriptor(kTestEventId), std::nullopt);
    ASSERT_EQ(engine->actionDescriptor(kTestActionId), std::nullopt);
}

TEST_F(EngineTest, invalidDescriptorMustNotBeRegistered)
{
    ItemDescriptor emptyDescriptor;

    ASSERT_FALSE(engine->registerEvent(emptyDescriptor, testEventConstructor));
    ASSERT_FALSE(engine->registerAction(emptyDescriptor, testActionConstructor));

    ASSERT_TRUE(engine->events().empty());
    ASSERT_TRUE(engine->actions().empty());
}

TEST_F(EngineTest, invalidConstructorMustNotBeRegistered)
{
    ASSERT_FALSE(engine->registerAction(TestEvent::manifest(), {}));
}

TEST_F(EngineTest, descriptionWithoutIdMustNotBeRegistered)
{
    ItemDescriptor descriptorWithoutId {.displayName = kDisplayName};

    ASSERT_FALSE(engine->registerEvent(descriptorWithoutId, testEventConstructor));
    ASSERT_FALSE(engine->registerAction(descriptorWithoutId, testActionConstructor));

    ASSERT_TRUE(engine->events().empty());
    ASSERT_TRUE(engine->actions().empty());
}

TEST_F(EngineTest, descriptionWithoutNameMustNotBeRegistered)
{
    ItemDescriptor descriptorWithoutName {.id = "nx.descriptor.id"};

    ASSERT_FALSE(engine->registerEvent(descriptorWithoutName, testEventConstructor));
    ASSERT_FALSE(engine->registerAction(descriptorWithoutName, testActionConstructor));

    ASSERT_TRUE(engine->events().empty());
    ASSERT_TRUE(engine->actions().empty());
}

TEST_F(EngineTest, validDescriptorMustBeRegistered)
{
    ItemDescriptor eventDescriptor{
        .id = kTestEventId,
        .displayName = kEventName,
        .description = TranslatableString("Event Description")};

    ASSERT_TRUE(engine->registerEvent(eventDescriptor, testEventConstructor));

    ASSERT_EQ(engine->events().size(), 1);

    ASSERT_EQ(engine->events().first().id, kTestEventId);
    ASSERT_EQ(engine->events().first().displayName, eventDescriptor.displayName);
    ASSERT_EQ(engine->events().first().description, eventDescriptor.description);

    ItemDescriptor actionDescriptor{
        .id = kTestActionId,
        .displayName = kActionName,
        .description = TranslatableString("Action Description")};

    ASSERT_TRUE(engine->registerAction(actionDescriptor, testActionConstructor));

    ASSERT_EQ(engine->actions().size(), 1);

    ASSERT_EQ(engine->actions().first().id, kTestActionId);
    ASSERT_EQ(engine->actions().first().displayName, actionDescriptor.displayName);
    ASSERT_EQ(engine->actions().first().description, actionDescriptor.description);
}

TEST_F(EngineTest, descriptorMustNotBeRegisteredIfSomeOfTheFieldsNotRegistered)
{
    const QString eventId = "nx.id";

    ItemDescriptor descriptorWithNotRegisteredField{
        .id = eventId,
        .displayName = kDisplayName,
        .description = {},
        .fields = {
            FieldDescriptor {
                .id = "nx.field.test",
                .fieldName = "testField",
                .displayName = kDisplayName,
            }
        }};

    // As an nx.field.test is not registered, an event filter must not be created for the
    // event descriptor contained such a field.
    ASSERT_FALSE(engine->registerEvent(descriptorWithNotRegisteredField, testEventConstructor));
    ASSERT_FALSE(engine->registerAction(descriptorWithNotRegisteredField, testActionConstructor));
}

TEST_F(EngineTest, eventFieldBuiltWithCorrectType)
{
    ASSERT_TRUE(engine->registerEventField(kTestEventFieldId, testEventFieldConstructor));

    auto field = engine->buildEventField(&kTestEventFieldDescriptor);

    ASSERT_TRUE(dynamic_cast<TestEventField*>(field.get()));
}

TEST_F(EngineTest, buildEventFieldMustFailIfFieldNotRegistered)
{
    auto field = engine->buildEventField(&kTestEventFieldDescriptor);

    ASSERT_FALSE(field);
}

TEST_F(EngineTest, eventFilterBuiltWithCorrectType)
{
    ASSERT_TRUE(engine->registerEventField(kTestEventFieldId, testEventFieldConstructor));

    const QString testFieldName = "testField";

    ItemDescriptor eventDescriptor{
        .id = kTestEventId,
        .displayName = kEventName,
        .description = {},
        .fields = {
            FieldDescriptor {
                .id = kTestEventFieldId,
                .fieldName = testFieldName,
                .displayName = kDisplayName,
            }
        }};

    ASSERT_TRUE(engine->registerEvent(eventDescriptor, testEventConstructor));

    auto eventFilter = engine->buildEventFilter(kTestEventId);

    ASSERT_TRUE(eventFilter);
    ASSERT_EQ(eventFilter->eventType(), kTestEventId);
    ASSERT_TRUE(eventFilter->fields().contains(testFieldName));
    ASSERT_TRUE(dynamic_cast<TestEventField*>(eventFilter->fields().value(testFieldName)));
}

TEST_F(EngineTest, buildEventFilterMustFailIfEventNotRegistered)
{
    auto eventFilter = engine->buildEventFilter(kTestEventId);

    ASSERT_FALSE(eventFilter);
}

TEST_F(EngineTest, manifestFieldNamesShouldBeUnique)
{
    const QString fieldId = fieldMetatype<TestEventField>();
    ASSERT_TRUE(engine->registerEventField(
        fieldId,
        [](const FieldDescriptor* descriptor){ return new TestEventField{descriptor}; }));

    ItemDescriptor eventDescriptor{
        .id = kTestEventId,
        .displayName = kDisplayName,
        .fields = {
            makeFieldDescriptor<TestEventField>("testField", kDisplayName),
            makeFieldDescriptor<TestEventField>("testField", kDisplayName),
        }
    };

    // Manifest fields should have different names.
    ASSERT_FALSE(engine->registerEvent(eventDescriptor, testEventConstructor));
}

TEST_F(EngineTest, actionFieldBuiltWithCorrectType)
{
    ASSERT_TRUE(Plugin::registerActionField<TestActionField>(engine.get()));

    auto field = engine->buildActionField(&kTestActionFieldDescriptor);

    ASSERT_TRUE(dynamic_cast<TestActionField*>(field.get()));
}

TEST_F(EngineTest, buildActionFieldMustFailIfFieldNotRegistered)
{
    auto field = engine->buildActionField(&kTestActionFieldDescriptor);

    ASSERT_FALSE(field);
}

TEST_F(EngineTest, actionBuilderBuiltWithCorrectTypeAndCorrectFields)
{
    ASSERT_TRUE(Plugin::registerActionField<TestActionField>(engine.get()));

    const QString fieldName = "testField";

    ItemDescriptor actionDescriptor{
        .id = kTestActionId,
        .displayName = kActionName,
        .description = {},
        .fields = {
            FieldDescriptor {
                .id = kTestActionFieldId,
                .fieldName = fieldName,
                .displayName = kDisplayName,
            }
        }};

    ASSERT_TRUE(engine->registerAction(actionDescriptor, testActionConstructor));

    auto actionBuilder = engine->buildActionBuilder(kTestActionId);

    ASSERT_TRUE(actionBuilder);
    ASSERT_EQ(actionBuilder->actionType(), kTestActionId);
    ASSERT_TRUE(actionBuilder->fields().contains(fieldName));
    ASSERT_TRUE(dynamic_cast<TestActionField*>(actionBuilder->fields().value(fieldName)));
}

TEST_F(EngineTest, buildActionBuilderMustFailIfActionNotRegistered)
{
    auto actionBuilder = engine->buildActionBuilder(kTestActionId);

    ASSERT_FALSE(actionBuilder);
}

TEST_F(EngineTest, updateRule)
{
    MockEngineEvents mockEvents(engine.get());
    auto ruleData1 = makeEmptyRuleData();

    EXPECT_CALL(mockEvents, onRuleAddedOrUpdated(ruleData1.id, true));
    EXPECT_CALL(mockEvents, onRuleAddedOrUpdated(ruleData1.id, false));
    engine->updateRule(ruleData1);
    engine->updateRule(ruleData1);
    EXPECT_TRUE(engine->rule(ruleData1.id));
}

TEST_F(EngineTest, removeRule)
{
    MockEngineEvents mockEvents(engine.get());
    auto ruleData1 = makeEmptyRuleData();

    EXPECT_CALL(mockEvents, onRuleAddedOrUpdated(ruleData1.id, true));
    EXPECT_CALL(mockEvents, onRuleRemoved(ruleData1.id));
    engine->updateRule(ruleData1);
    engine->removeRule(ruleData1.id);
    EXPECT_TRUE(engine->rules().empty());
    engine->removeRule(ruleData1.id);
}

TEST_F(EngineTest, resetRules)
{
    MockEngineEvents mockEvents(engine.get());
    auto ruleData1 = makeEmptyRuleData();

    EXPECT_CALL(mockEvents, onRulesReset());
    EXPECT_EQ(engine->rules().size(), 0);
    engine->resetRules({});

    EXPECT_CALL(mockEvents, onRuleAddedOrUpdated(ruleData1.id, true));
    EXPECT_CALL(mockEvents, onRulesReset());
    engine->updateRule(ruleData1);
    engine->resetRules({});
}

TEST_F(EngineTest, cloneEvent)
{
    ASSERT_TRUE(engine->registerEvent(TestEvent::manifest(), testEventConstructor));

    const auto original = TestEventPtr::create(
        std::chrono::microseconds(123456),
        State::instant);

    original->m_cameraId = nx::Uuid::createUuid();
    original->m_text = "Test text";
    original->m_intField = 42;
    original->m_floatField = 3.14f;

    const auto basicCopy = engine->cloneEvent(original);
    ASSERT_TRUE(basicCopy);

    const auto copy = basicCopy.dynamicCast<TestEvent>();
    EXPECT_TRUE(copy);
    EXPECT_EQ(original->type(), copy->type());
    EXPECT_NE(original, copy);

    for (const auto& propName :
        nx::utils::propertyNames(original.get(), nx::utils::PropertyAccess::readable))
    {
        static const QSet<QString> kNoComparator = {"attributes"};
        if (kNoComparator.contains(propName))
            continue;

        SCOPED_TRACE(nx::format("Prop name: %1", propName).toStdString());
        EXPECT_EQ(original->property(propName), copy->property(propName));
    }
}

TEST_F(EngineTest, defaultFieldBuiltForAbsentInManifest)
{
    // Suppose there is one event and one action registered. Both have two fields each.

    const QString eventFieldType = fieldMetatype<TestEventField>();
    engine->registerEventField(
        eventFieldType,
        [](const FieldDescriptor* descriptor) { return new TestEventField{descriptor}; });

    ItemDescriptor eventDescriptor{
        .id = kTestEventId,
        .displayName = kDisplayName,
        .fields = {
            makeFieldDescriptor<TestEventField>("testField1", kDisplayName),
            makeFieldDescriptor<TestEventField>("testField2", kDisplayName),
        }
    };
    engine->registerEvent(eventDescriptor, testEventConstructor);

    const QString actionFieldType = fieldMetatype<TestActionField>();
    Plugin::registerActionField<TestActionField>(engine.get());

    ItemDescriptor actionDescriptor{
        .id = kTestActionId,
        .displayName = kDisplayName,
        .fields = {
            makeFieldDescriptor<TestActionField>("testField1", kDisplayName),
            makeFieldDescriptor<TestActionField>("testField2", kDisplayName),
        }
    };
    engine->registerAction(actionDescriptor, testActionConstructor);

    // Received serialized event filter and action builder both have only one field each.

    vms::api::rules::EventFilter serializedEventFilter {
        .id = nx::Uuid::createUuid(),
        .type = kTestEventId,
        .fields = {
            { "testField1", {.type = eventFieldType} }
        }
    };

    vms::api::rules::ActionBuilder serializedActionBuilder {
        .id = nx::Uuid::createUuid(),
        .type = kTestActionId,
        .fields = {
            { "testField1", {.type = actionFieldType} }
        }
    };

    // Expect that built event filter and action builder contains absence field.

    auto eventFilter = engine->buildEventFilter(serializedEventFilter);
    ASSERT_TRUE(eventFilter);
    ASSERT_TRUE(eventFilter->fields().contains("testField1"));
    ASSERT_EQ(eventFilter->fields()["testField1"]->metatype(), eventFieldType);
    ASSERT_TRUE(eventFilter->fields().contains("testField2"));
    ASSERT_EQ(eventFilter->fields()["testField2"]->metatype(), eventFieldType);

    auto actionBuilder = engine->buildActionBuilder(serializedActionBuilder);
    ASSERT_TRUE(actionBuilder);
    ASSERT_TRUE(actionBuilder->fields().contains("testField1"));
    ASSERT_EQ(actionBuilder->fields()["testField1"]->metatype(), actionFieldType);
    ASSERT_TRUE(actionBuilder->fields().contains("testField2"));
    ASSERT_EQ(actionBuilder->fields()["testField2"]->metatype(), actionFieldType);
}

TEST_F(EngineTest, onlyValidProlongedActionRegistered)
{
    ASSERT_TRUE(Plugin::registerActionField<OptionalTimeField>(engine.get()));

    ItemDescriptor withoutFields{
        .id = "withoutFields",
        .displayName = kDisplayName,
        .flags = ItemFlag::prolonged,
    };
    ASSERT_TRUE(engine->registerAction(withoutFields, testActionConstructor));

    ItemDescriptor withIntervalField{
        .id = "withIntervalField",
        .displayName = kDisplayName,
        .flags = ItemFlag::prolonged,
        .fields = {
            makeFieldDescriptor<OptionalTimeField>(
                utils::kIntervalFieldName,
                TranslatableString("Interval"),
                {})
        }
    };
    ASSERT_FALSE(engine->registerAction(withIntervalField, testActionConstructor));

    ItemDescriptor withIntervalAndDurationFields{
        .id = "withIntervalAndDurationFields",
        .displayName = kDisplayName,
        .flags = ItemFlag::prolonged,
        .fields = {
            makeFieldDescriptor<OptionalTimeField>(
                utils::kIntervalFieldName,
                TranslatableString("Interval"),
                {}),
            makeFieldDescriptor<OptionalTimeField>(
                utils::kDurationFieldName,
                TranslatableString("Duration"),
                {})
        }
    };
    ASSERT_TRUE(engine->registerAction(withIntervalAndDurationFields, testActionConstructor));
}

TEST_F(EngineTest, runningEventState)
{
   auto plugin = TestPlugin(engine.get());

    engine->registerEventField(
        fieldMetatype<StateField>(),
        [](const FieldDescriptor* descriptor) { return new StateField{descriptor}; });

    auto rule = makeRule<TestEventProlonged, TestAction>();

    auto stateField = rule->eventFilters().first()->fieldByName<StateField>(utils::kStateFieldName);
    stateField->setValue(State::started);

    // Register two identical rules.
    engine->updateRule(serialize(rule.get()));
    rule->setId(nx::Uuid::createUuid());
    engine->updateRule(serialize(rule.get()));

    auto event = TestEventProlongedPtr::create(std::chrono::microseconds::zero(), State::started);

    // Event state is cached.
    EXPECT_EQ(engine->processEvent(event), 2);
    EXPECT_TRUE(engine->runningEventWatcher(rule->id()).isRunning(event));
    // As state is not changed, the event must be ignored.
    EXPECT_EQ(engine->processEvent(event), 0);

    event->setState(State::stopped);
    EXPECT_EQ(engine->processEvent(event), 0);
    // After the event is processed it must be removed from the event cache.
    EXPECT_FALSE(engine->runningEventWatcher(rule->id()).isRunning(event));
}

TEST_F(EngineTest, instantEventsAreNotAddedToRunningEventWatcher)
{
    // Makes rule accepted any event.
    auto rule = makeRule<TestEvent, TestAction>();

    // Register two such rules.
    engine->updateRule(serialize(rule.get()));
    rule->setId(nx::Uuid::createUuid());
    engine->updateRule(serialize(rule.get()));

    auto event = TestEventPtr::create(std::chrono::microseconds::zero(), State::instant);

    EXPECT_EQ(engine->processEvent(event), 2);
    EXPECT_FALSE(engine->runningEventWatcher(rule->id()).isRunning(event));
    EXPECT_EQ(engine->processEvent(event), 2);
    EXPECT_FALSE(engine->runningEventWatcher(rule->id()).isRunning(event));
}

TEST_F(EngineTest, notStartedEventIsNotProcessed)
{
    engine->registerEvent(TestEvent::manifest(), testEventConstructor);

    // The rule accept any event.
    auto rule1 = std::make_unique<Rule>(nx::Uuid::createUuid(), engine.get());
    rule1->addEventFilter(engine->buildEventFilter(utils::type<TestEvent>()));
    engine->updateRule(serialize(rule1.get()));

    auto event = TestEventPtr::create(std::chrono::microseconds::zero(), State::stopped);

    // Event with stopped state without previous started state is ignored.
    EXPECT_EQ(engine->processEvent(event), 0);
}

TEST_F(EngineTest, timerThread)
{
    auto plugin = TestPlugin(engine.get());
    auto executor = TestActionExecutor();
    auto connector = TestEventConnector();
    engine->addActionExecutor(utils::type<TestActionWithInterval>(), &executor);
    engine->addEventConnector(&connector);

    QThread thread;
    thread.setObjectName("Engine thread");
    thread.start();
    engine->moveToThread(&thread);

    auto ruleId = nx::Uuid::createUuid();
    {
        auto rule1 = std::make_unique<Rule>(ruleId, engine.get());
        rule1->addEventFilter(engine->buildEventFilter(utils::type<TestEvent>()));
        auto builder = engine->buildActionBuilder(utils::type<TestActionWithInterval>());
        ASSERT_TRUE(builder);
        builder->fieldByType<OptionalTimeField>()->setValue(30s);
        rule1->addActionBuilder(std::move(builder));
        engine->updateRule(serialize(rule1.get()));
    }

    // Event1 will be processed, event2 will be aggregated, timer will be started.
    auto event1 = TestEventPtr::create(syncTime.currentTimePoint(), State::instant);
    auto event2 = TestEventPtr::create(syncTime.currentTimePoint(), State::instant);

    connector.process(event1);
    connector.process(event2);

    for (int i = 0; i < 600; ++i)
    {
        if (executor.count > 0)
            break;

        std::this_thread::sleep_for(50ms);
    }

    ASSERT_EQ(executor.actions.size(), 1);

    // Timer will be stopped on rule removal in engine's thread.
    engine->removeRule(ruleId);

    executeInThread(
        &thread,
        [this, &thread]
        {
            engine.reset();
            thread.quit();
        });

    thread.wait();
}

} // namespace nx::vms::rules::test
