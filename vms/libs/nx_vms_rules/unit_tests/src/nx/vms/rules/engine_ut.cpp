// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QtCore/QThread>

#include <nx/utils/async_handler_executor.h>
#include <nx/utils/qobject.h>
#include <nx/vms/api/rules/rule.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_fields/optional_time_field.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils/api.h>
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

const QString kTestEventId = "nx.events.testEvent";
const QString kTestActionId = "nx.actions.testAction";
const QString kTestEventFieldId = "nx.actions.field.test";
const QString kTestActionFieldId = "nx.events.field.test";

api::Rule makeEmptyRuleData()
{
    return {{QnUuid::createUuid()}};
}

} // namespace

class EngineTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        engine = std::make_unique<Engine>(std::make_unique<TestRouter>());
    }

    virtual void TearDown() override
    {
        engine.reset();
    }

    QnSyncTime syncTime;
    std::unique_ptr<Engine> engine;
    Engine::EventConstructor testEventConstructor = [] { return new TestEvent; };
    Engine::ActionConstructor testActionConstructor = [] { return new TestAction; };
    Engine::EventFieldConstructor testEventFieldConstructor = [] { return new TestEventField; };
    Engine::ActionFieldConstructor testActionFieldConstructor = [] { return new TestActionField; };
};

TEST_F(EngineTest, ruleAddedSuccessfully)
{
    // No rules at the moment the engine is just created.
    ASSERT_EQ(0, engine->rules().size());

    auto rule = std::make_unique<Rule>(QnUuid::createUuid(), engine.get());
    engine->updateRule(serialize(rule.get()));

    ASSERT_EQ(engine->rules().size(), 1);
    ASSERT_TRUE(engine->rule(rule->id()));
}

TEST_F(EngineTest, ruleClonedSuccessfully)
{
    auto rule = std::make_unique<Rule>(QnUuid::createUuid(), engine.get());
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
    ItemDescriptor descriptorWithoutId {.displayName = "Name"};

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
        .displayName = "Event Name",
        .description = "Event Description"};

    ASSERT_TRUE(engine->registerEvent(eventDescriptor, testEventConstructor));

    ASSERT_EQ(engine->events().size(), 1);

    ASSERT_EQ(engine->events().first().id, kTestEventId);
    ASSERT_EQ(engine->events().first().displayName, eventDescriptor.displayName);
    ASSERT_EQ(engine->events().first().description, eventDescriptor.description);

    ItemDescriptor actionDescriptor{
        .id = kTestActionId,
        .displayName = "Action Name",
        .description = "Action Description"};

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
        .displayName = "Display Name",
        .description = "Description",
        .fields = {
            FieldDescriptor {
                .id = "nx.field.test",
                .fieldName = "testField",
                .displayName = "Test Field",
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

    auto field = engine->buildEventField(kTestEventFieldId);

    ASSERT_TRUE(dynamic_cast<TestEventField*>(field.get()));
}

TEST_F(EngineTest, buildEventFieldMustFailIfFieldNotRegistered)
{
    auto field = engine->buildEventField(kTestEventFieldId);

    ASSERT_FALSE(field);
}

TEST_F(EngineTest, eventFilterBuiltWithCorrectType)
{
    ASSERT_TRUE(engine->registerEventField(kTestEventFieldId, testEventFieldConstructor));

    const QString testFieldName = "testField";

    ItemDescriptor eventDescriptor{
        .id = kTestEventId,
        .displayName = "Event Name",
        .description = "Event Description",
        .fields = {
            FieldDescriptor {
                .id = kTestEventFieldId,
                .fieldName = testFieldName,
                .displayName = "Test Field",
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
    ASSERT_TRUE(engine->registerEventField(fieldId, []{ return new TestEventField; }));

    const QString eventId = "nx.events.test";
    ItemDescriptor eventDescriptor{
        .id = eventId,
        .displayName = "Test Name",
        .fields = {
            makeFieldDescriptor<TestEventField>("testField", "Test Field"),
            makeFieldDescriptor<TestEventField>("testField", "Test Field"),
        }
    };

    // Manifest fields should have different names.
    ASSERT_FALSE(engine->registerEvent(eventDescriptor, testEventConstructor));
}

TEST_F(EngineTest, actionFieldBuiltWithCorrectType)
{
    ASSERT_TRUE(engine->registerActionField(kTestActionFieldId, testActionFieldConstructor));

    auto field = engine->buildActionField(kTestActionFieldId);

    ASSERT_TRUE(dynamic_cast<TestActionField*>(field.get()));
}

TEST_F(EngineTest, buildActionFieldMustFailIfFieldNotRegistered)
{
    auto field = engine->buildActionField(kTestActionFieldId);

    ASSERT_FALSE(field);
}

TEST_F(EngineTest, actionBuilderBuiltWithCorrectTypeAndCorrectFields)
{
    ASSERT_TRUE(engine->registerActionField(kTestActionFieldId, testActionFieldConstructor));

    const QString fieldName = "testField";

    ItemDescriptor actionDescriptor{
        .id = kTestActionId,
        .displayName = "Action Name",
        .description = "Action Description",
        .fields = {
            FieldDescriptor {
                .id = kTestActionFieldId,
                .fieldName = fieldName,
                .displayName = "Test Field",
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

    original->m_cameraId = QnUuid::createUuid();
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
    engine->registerEventField(eventFieldType, []{ return new TestEventField; });

    const QString eventType = "nx.events.test";
    ItemDescriptor eventDescriptor{
        .id = eventType,
        .displayName = "Test Name",
        .fields = {
            makeFieldDescriptor<TestEventField>("testField1", "Test Field 1"),
            makeFieldDescriptor<TestEventField>("testField2", "Test Field 2"),
        }
    };
    engine->registerEvent(eventDescriptor, testEventConstructor);

    const QString actionFieldType = fieldMetatype<TestActionField>();
    engine->registerActionField(actionFieldType, []{ return new TestActionField; });

    const QString actionType = "nx.actions.test";
    ItemDescriptor actionDescriptor{
        .id = actionType,
        .displayName = "Test Name",
        .fields = {
            makeFieldDescriptor<TestActionField>("testField1", "Test Field 1"),
            makeFieldDescriptor<TestActionField>("testField2", "Test Field 2"),
        }
    };
    engine->registerAction(actionDescriptor, testActionConstructor);

    // Received serialized event filter and action builder both have only one field each.

    vms::api::rules::EventFilter serializedEventFilter {
        .id = QnUuid::createUuid(),
        .type = eventType,
        .fields = {
            { "testField1", {.type = eventFieldType} }
        }
    };

    vms::api::rules::ActionBuilder serializedActionBuilder {
        .id = QnUuid::createUuid(),
        .type = actionType,
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
    ASSERT_TRUE(engine->registerActionField(
        fieldMetatype<OptionalTimeField>(), []{ return new OptionalTimeField{}; }));

    ItemDescriptor withoutFields{
        .id = "nx.actions.withoutFields",
        .displayName = "Test Name",
        .flags = ItemFlag::prolonged,
    };
    ASSERT_TRUE(engine->registerAction(withoutFields, testActionConstructor));

    ItemDescriptor withIntervalField{
        .id = "nx.actions.withIntervalField",
        .displayName = "Test Name",
        .flags = ItemFlag::prolonged,
        .fields = {
            makeFieldDescriptor<OptionalTimeField>(utils::kIntervalFieldName, "Interval", {})
        }
    };
    ASSERT_FALSE(engine->registerAction(withIntervalField, testActionConstructor));

    ItemDescriptor withIntervalAndDurationFields{
        .id = "nx.actions.withIntervalAndDurationFields",
        .displayName = "Test Name",
        .flags = ItemFlag::prolonged,
        .fields = {
            makeFieldDescriptor<OptionalTimeField>(utils::kIntervalFieldName, "Interval", {}),
            makeFieldDescriptor<OptionalTimeField>(utils::kDurationFieldName, "Duration", {})
        }
    };
    ASSERT_TRUE(engine->registerAction(withIntervalAndDurationFields, testActionConstructor));
}

TEST_F(EngineTest, cacheKey)
{
    engine->registerEvent(TestEvent::manifest(), testEventConstructor);

    auto rule = std::make_unique<Rule>(QnUuid::createUuid(), engine.get());
    rule->addEventFilter(engine->buildEventFilter(utils::type<TestEvent>()));
    engine->updateRule(serialize(rule.get()));

    auto event = TestEventPtr::create(std::chrono::microseconds::zero(), State::instant);

    // Empty cache key events are not cached.
    EXPECT_EQ(engine->processEvent(event), 1);
    EXPECT_EQ(engine->processEvent(event), 1);

    // Same cache events are ignored.
    event->setCacheKey("a");
    EXPECT_EQ(engine->processEvent(event), 1);
    EXPECT_EQ(engine->processEvent(event), 0);

    // Different cache events are matched.
    event->setCacheKey("b");
    EXPECT_EQ(engine->processEvent(event), 1);
    EXPECT_EQ(engine->processEvent(event), 0);
}

TEST_F(EngineTest, cacheTimeout)
{
    using namespace std::chrono;

    engine->registerEvent(TestEvent::manifest(), testEventConstructor);

    auto rule = std::make_unique<Rule>(QnUuid::createUuid(), engine.get());
    rule->addEventFilter(engine->buildEventFilter(utils::type<TestEvent>()));
    engine->updateRule(serialize(rule.get()));

    auto event = TestEventPtr::create(std::chrono::microseconds::zero(), State::instant);

    // Same cache events are ignored.
    event->setCacheKey("a");
    EXPECT_EQ(engine->processEvent(event), 1);
    EXPECT_EQ(engine->processEvent(event), 0);

    // Same cache events are matched after timeout.
    std::this_thread::sleep_for(5ms);
    engine->eventCache()->cleanupOldEventsFromCache(1ms, 1ms);
    EXPECT_EQ(engine->processEvent(event), 1);
    EXPECT_EQ(engine->processEvent(event), 0);
}

TEST_F(EngineTest, cacheState)
{
    engine->registerEvent(TestEvent::manifest(), testEventConstructor);

    // Both rules accept any event.
    auto rule1 = std::make_unique<Rule>(QnUuid::createUuid(), engine.get());
    rule1->addEventFilter(engine->buildEventFilter(utils::type<TestEvent>()));
    engine->updateRule(serialize(rule1.get()));

    auto rule2 = std::make_unique<Rule>(QnUuid::createUuid(), engine.get());
    rule2->addEventFilter(engine->buildEventFilter(utils::type<TestEvent>()));
    engine->updateRule(serialize(rule2.get()));

    auto event = TestEventPtr::create(std::chrono::microseconds::zero(), State::started);

    // Event state is cached.
    EXPECT_EQ(engine->processEvent(event), 2);
    // As state is not changed, ignore the event.
    EXPECT_EQ(engine->processEvent(event), 0);

    // New state must be handled and cached.
    event->setState(State::stopped);
    EXPECT_EQ(engine->processEvent(event), 2);
    // Cached state is ignored.
    EXPECT_EQ(engine->processEvent(event), 0);

    // State is not cached for the instant event.
    event->setState(State::instant);
    EXPECT_EQ(engine->processEvent(event), 2);
    EXPECT_EQ(engine->processEvent(event), 2);
}

TEST_F(EngineTest, notStartedEventIsNotProcessed)
{
    engine->registerEvent(TestEvent::manifest(), testEventConstructor);

    // The rule accept any event.
    auto rule1 = std::make_unique<Rule>(QnUuid::createUuid(), engine.get());
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
    thread.start();
    engine->moveToThread(&thread);

    auto ruleId = QnUuid::createUuid();
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

    std::this_thread::sleep_for(50ms);
    ASSERT_EQ(executor.actions.size(), 1);

    // Timer will be stopped on rule removal in engine's thread.
    engine->removeRule(ruleId);

    nx::utils::AsyncHandlerExecutor(&thread).submit(
        [this, &thread]
        {
            engine.reset();
            thread.quit();
        });

    thread.wait();
}

} // namespace nx::vms::rules::test
