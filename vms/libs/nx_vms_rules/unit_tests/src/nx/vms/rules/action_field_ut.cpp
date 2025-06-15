// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>

#include <gtest/gtest.h>

#include <QtCore/QString>

#include <nx/vms/api/rules/rule.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_fields/builtin_fields.h>
#include <nx/vms/rules/action_executor.h>
#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/builtin_fields.h>
#include <nx/vms/rules/events/builtin_events.h>
#include <nx/vms/rules/plugin.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils/api.h>
#include <nx/vms/rules/utils/event_parameter_helper.h>

#include "test_action.h"
#include "test_event.h"
#include "test_infra.h"
#include "test_router.h"

namespace nx::vms::rules::test {

namespace {

template<class Field, class T>
void testSimpleTypeField(const std::initializer_list<T>& values)
{
    using ValueType = typename Field::value_type;

    for (const auto& testValue: values)
    {
        static const FieldDescriptor fakeDescriptor;
        Field field{&fakeDescriptor};
        SCOPED_TRACE(nx::format(
            "Field type: %1, value: %2",
            field.metatype(),
            testValue).toStdString());

        EXPECT_EQ(field.value(), ValueType());

        ValueType value(testValue);
        field.setValue(value);
        EXPECT_EQ(field.value(), value);

        QVariant result = field.build({});
        EXPECT_TRUE(result.canConvert<ValueType>());
        EXPECT_EQ(value, result.value<ValueType>());
    }
}

} // namespace

TEST(SimpleTypeActionFieldTest, SimpleTypes)
{
    testSimpleTypeField<ActionFlagField>({true, false});
    testSimpleTypeField<OptionalTimeField>({-1, 0, 60, 300, 86400});
    testSimpleTypeField<ActionTextField>({ "", "Hello", "\\/!@#$%^&*()_+" });
}

struct FormatResult
{
    QString expected;
    QString format;
};

class ActionFieldTest:
    public EngineBasedTest,
    public ::testing::WithParamInterface<FormatResult>
{
public:
    const EventData kEventData = {
        {"type", TestEvent::manifest().id},
        {"intField", 123},
        {"text", "hello"},
        {"boolField", true},
        {"floatField", 123.123},
        {rules::utils::kDeviceIdFieldName, nx::Uuid::createUuid().toSimpleString()},
    };

    const FieldDescriptor kDummyDescriptor;

    ActionFieldTest()
    {
        engine->registerEventField(
            fieldMetatype<StateField>(),
            [](const FieldDescriptor* descriptor) { return new StateField(descriptor); });

        engine->registerEvent(TestEvent::manifest(), []{ return new TestEvent(); });
    }

    TestEventPtr makeEvent(State state = State::instant)
    {
        auto event = engine->buildEvent(kEventData).staticCast<TestEvent>();
        auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch());
        event->setTimestamp(timestamp);
        event->setState(state);
        return event;
    }

    EventPtr makeAnalyticsEvent()
    {
        engine->registerEventField(
            fieldMetatype<SourceCameraField>(),
            [](const FieldDescriptor* descriptor) { return new SourceCameraField(descriptor); });
        engine->registerEventField(
            fieldMetatype<AnalyticsEventTypeField>(),
            [this](const FieldDescriptor* descriptor)
            {
                return new AnalyticsEventTypeField(systemContext(), descriptor);
            });
        engine->registerEventField(
            fieldMetatype<TextLookupField>(),
            [this](const FieldDescriptor* descriptor)
            {
                return new TextLookupField(systemContext(), descriptor);
            });
        engine->registerEventField(
            fieldMetatype<AnalyticsAttributesField>(),
            [this](const FieldDescriptor* descriptor)
            {
                return new AnalyticsAttributesField(descriptor);
            });
        engine->registerEvent(AnalyticsEvent::manifest(), [] { return new AnalyticsEvent(); });

        static const EventData kEventAtributeData = {
            {"type", AnalyticsEvent::manifest().id}
        };
        return engine->buildEvent(kEventAtributeData);
    }

    EventPtr makeSoftTriggerEvent()
    {
        engine->registerEventField(
            fieldMetatype<UniqueIdField>(),
            [](const FieldDescriptor* descriptor) { return new UniqueIdField(descriptor); });
        engine->registerEventField(
            fieldMetatype<SourceCameraField>(),
            [](const FieldDescriptor* descriptor) { return new SourceCameraField(descriptor); });
        engine->registerEventField(
            fieldMetatype<SourceUserField>(),
            [this](const FieldDescriptor* descriptor)
            {
                return new SourceUserField(systemContext(), descriptor);
            });
        engine->registerEventField(
            fieldMetatype<CustomizableTextField>(),
            [](const FieldDescriptor* descriptor)
            {
                return new CustomizableTextField(descriptor);
            });
        engine->registerEventField(
            fieldMetatype<CustomizableIconField>(),
            [](const FieldDescriptor* descriptor)
            {
                return new CustomizableIconField(descriptor);
            });
        engine->registerEvent(
            SoftTriggerEvent::manifest(),
            [](){ return new SoftTriggerEvent(); });

        static const EventData kEventAtributeData = {
            {"type", SoftTriggerEvent::manifest().id},
        };
        return engine->buildEvent(kEventAtributeData);
    }

};

TEST_F(ActionFieldTest, EventTimeInstantEvent)
{
    TextWithFields field(systemContext(), &kDummyDescriptor);
    field.setText("{event.time}");
    auto event = AggregatedEventPtr::create(makeEvent());
    EXPECT_EQ(nx::utils::timestampToISO8601(event->timestamp()), field.build(event).toString());
}

TEST_F(ActionFieldTest, EventTimestamp)
{
    TextWithFields field(systemContext(), &kDummyDescriptor);
    field.setText("{event.timestamp}");
    auto event = AggregatedEventPtr::create(makeEvent());
    const auto eventTimeStamp = reflect::toString(
        std::chrono::duration_cast<std::chrono::seconds>(event->timestamp()).count());
    EXPECT_EQ(eventTimeStamp, field.build(event).toString());
}

TEST_F(ActionFieldTest, EventTimestampUs)
{
    TextWithFields field(systemContext(), &kDummyDescriptor);
    field.setText("{event.timestampUs}");
    auto event = AggregatedEventPtr::create(makeEvent());
    const auto eventTimeStamp = reflect::toString(
        std::chrono::duration_cast<std::chrono::microseconds>(event->timestamp()).count());
    EXPECT_EQ(eventTimeStamp, field.build(event).toString());
}

TEST_F(ActionFieldTest, EventTimestampMs)
{
    TextWithFields field(systemContext(), &kDummyDescriptor);
    field.setText("{event.timestampMs}");
    auto event = AggregatedEventPtr::create(makeEvent());
    const auto eventTimeStamp = reflect::toString(
        std::chrono::duration_cast<std::chrono::milliseconds>(event->timestamp()).count());
    EXPECT_EQ(eventTimeStamp, field.build(event).toString());
}

TEST_F(ActionFieldTest, EventStartTimeInstantEvent)
{
    TextWithFields fieldStartTime(systemContext(), &kDummyDescriptor);
    QString arg = "{event.time.start}";
    fieldStartTime.setText(arg);
    // There is no start time for Instant event, so there is no substitution.
    EXPECT_EQ(arg, fieldStartTime.build(AggregatedEventPtr::create(makeEvent())).toString());
}

TEST_F(ActionFieldTest, EventTimeProlongedEvent)
{
    Plugin::registerActionFieldWithEngine<TextWithFields>(engine.get(), systemContext());

    engine->registerAction(
        TestActionWithTextWithFields::manifest(),
        [] { return new TestActionWithTextWithFields{}; });

    auto eventFilter = engine->buildEventFilter(utils::type<TestEvent>());

    auto actionBuilder = engine->buildActionBuilder(utils::type<TestActionWithTextWithFields>());
    auto textWithFieldsField =
        actionBuilder->fieldByName<TextWithFields>(TestActionWithTextWithFields::kFieldName);
    textWithFieldsField->setText(
        QString{"{event.time.start}%1{event.time}%1{event.time.end}"}.arg("|"));

    auto rule = std::make_unique<Rule>(nx::Uuid::createUuid(), engine.get());
    rule->addEventFilter(std::move(eventFilter));
    rule->addActionBuilder(std::move(actionBuilder));

    engine->updateRule(serialize(rule.get()));

    TestActionExecutor executor;
    engine->addActionExecutor(utils::type<TestActionWithTextWithFields>(), &executor);

    auto eventStart = makeEvent(State::started);
    engine->processEvent(eventStart);

    auto eventEnd = makeEvent(State::stopped);
    engine->processEvent(eventEnd);

    EXPECT_EQ(executor.actions.size(), 2);

    struct Timestamps
    {
        QString startTime, activeTime, endTime;

        static Timestamps fromAction(TestActionWithTextWithFields* action)
        {
            const auto split = action->m_text.split("|");
            return Timestamps{split[0], split[1], split[2]};
        }
    };

    const auto startedAction =
        dynamic_cast<TestActionWithTextWithFields*>(executor.actions.at(0).get());
    const auto startedActionTimestamps = Timestamps::fromAction(startedAction);

    EXPECT_TRUE(!startedActionTimestamps.startTime.isEmpty()
        && !startedActionTimestamps.activeTime.isEmpty());
    EXPECT_TRUE(startedActionTimestamps.endTime.isEmpty()); //< Event doesn't have end time yet.
    // Started and active time are equal for the started entity.
    EXPECT_EQ(startedActionTimestamps.startTime, startedActionTimestamps.activeTime);

    const auto stoppedAction =
        dynamic_cast<TestActionWithTextWithFields*>(executor.actions.at(1).get());
    const auto stoppedActionTimestamps = Timestamps::fromAction(stoppedAction);

    EXPECT_TRUE(!stoppedActionTimestamps.startTime.isEmpty()
        && !stoppedActionTimestamps.activeTime.isEmpty()
        && !stoppedActionTimestamps.endTime.isEmpty());
    EXPECT_EQ(stoppedActionTimestamps.startTime, startedActionTimestamps.startTime);
    // End and active time are equal for the stopped entity.
    EXPECT_EQ(stoppedActionTimestamps.activeTime, stoppedActionTimestamps.endTime);
}

TEST_F(ActionFieldTest, EventDotType)
{
    TextWithFields field(systemContext(), &kDummyDescriptor);
    field.setText("{event.type}");
    EXPECT_EQ(
        AnalyticsEvent::manifest().id,
        field.build(AggregatedEventPtr::create(makeAnalyticsEvent())).toString());
}

/** {event.name} always uses name from the manifest. */
TEST_F(ActionFieldTest, EventDotName)
{
    TextWithFields field(systemContext(), &kDummyDescriptor);
    field.setText("{event.name}");

    { // Test Event.
        EXPECT_EQ(
            TestEvent::manifest().displayName,
            field.build(AggregatedEventPtr::create(makeEvent())).toString());
    }
    { // Event with custom caption.
        auto event = makeAnalyticsEvent();
        event->setProperty("caption", "Test caption");
        EXPECT_EQ(AnalyticsEvent::manifest().displayName,
            field.build(AggregatedEventPtr::create(event)).toString());
    }
}

/**
 * {event.caption} uses user-provided caption, and if it is absent - name from the manifest. There
 * is an exception for the Analytics Event, which can also use taxonomy, but this is not validated
 * in scope of this test.
 */
TEST_F(ActionFieldTest, EventDotCaption)
{
    auto event = makeAnalyticsEvent();
    TextWithFields field(systemContext(), &kDummyDescriptor);

    field.setText("{event.caption}");
    EXPECT_EQ(AnalyticsEvent::manifest().displayName,
        field.build(AggregatedEventPtr::create(event)).toString());

    static const QString kEventCaption = "Test caption";
    event->setProperty("caption", kEventCaption);

    EXPECT_EQ(kEventCaption, field.build(AggregatedEventPtr::create(event)).toString());
}

/** {event.description} uses only user-provided description in any case. */
TEST_F(ActionFieldTest, EventDotDescription)
{
    TextWithFields field(systemContext(), &kDummyDescriptor);
    auto event = makeAnalyticsEvent();

    field.setText("{event.description}");
    EXPECT_EQ(QString(), field.build(AggregatedEventPtr::create(event)).toString());

    static const QString kEventDescription = "Test description override";
    event->setProperty("description", kEventDescription);

    EXPECT_EQ(kEventDescription, field.build(AggregatedEventPtr::create(event)).toString());
}

TEST_F(ActionFieldTest, EventDotSource)
{
    TextWithFields field(systemContext(), &kDummyDescriptor);
    auto event = makeEvent();
    auto eventAggregator = AggregatedEventPtr::create(event);

    field.setText("{event.source}");

    EXPECT_EQ("Test resource", field.build(eventAggregator).toString());
}

TEST_F(ActionFieldTest, EventTooltip)
{
    TextWithFields field(systemContext(), &kDummyDescriptor);

    field.setText("{event.extendedDescription}");

    const auto tooltip = field.build(AggregatedEventPtr::create(makeEvent())).toString();
    EXPECT_NE(tooltip, field.text());
    // Tooltip must not be empty, at least event name and timestamp must exists.
    EXPECT_FALSE(tooltip.isEmpty());
}

TEST_P(ActionFieldTest, FormatLine)
{
    const auto [expected, format] = GetParam();

    TextWithFields field(systemContext(), &kDummyDescriptor);
    field.setText(format);
    SCOPED_TRACE(nx::format("Format line: %1", field.text()).toStdString());

    auto result = field.build(AggregatedEventPtr::create(makeEvent()));
    ASSERT_TRUE(result.isValid());

    EXPECT_EQ(expected.toStdString(), result.toString().toStdString());
}

static const std::vector<FormatResult> kFormatResults
{
    // Empty format string.
    {"", ""},

    // Test valid formatting
    {"int 123", "int {event.fields.intField}"},
    {"int {} int", "int {} int"},
    {"string hello", "string {event.fields.text}"},
    {"bool true", "bool {event.fields.boolField}"},
    {"float 123.123", "float {event.fields.floatField}"},
    {"123 hello true 123.123",
    "{event.fields.intField} {event.fields.text} {event.fields.boolField} {event.fields.floatField}"},

    // Test placeholders.
    {"123 {absent}", "{event.fields.intField} {absent}"},

    // Test invalid format string.
    {"int {int", "int {int"},
    {"int} int", "int} int"},
    {"int {} int", "int {} int"},

    // Test exotic cases.
    {"{{123}}", "{{{event.fields.intField}}}"},
    {"}{{123}}", "}{{{event.fields.intField}}}"},
    {"{{123}}{", "{{{event.fields.intField}}}{"},
    {"{ { 123}}", "{ { {event.fields.intField}}}"},
    {"{ 123}}", "{ {event.fields.intField}}}"},
    {"{ {int{}}}", "{ {int{}}}"},
};

INSTANTIATE_TEST_SUITE_P(CommonSet,
    ActionFieldTest,
    ::testing::ValuesIn(kFormatResults));

TEST_F(ActionFieldTest, TargetUsersField)
{
    UuidSelection selection;
    selection.ids << nx::Uuid::createUuid() << nx::Uuid::createUuid();
    selection.all = true;

    TargetUsersField field(systemContext(), &kDummyDescriptor);
    field.setIds(selection.ids);
    field.setAcceptAll(selection.all);

    EXPECT_EQ(selection.ids, field.ids());
    EXPECT_EQ(selection.all, field.acceptAll());

    auto result = field.build(/*eventData*/ {});
    EXPECT_TRUE(result.canConvert<UuidSelection>());

    EXPECT_EQ(selection, result.value<UuidSelection>());
}

TEST_F(ActionFieldTest, TargetSingleDeviceTest)
{
    const auto event = QSharedPointer<TestEvent>::create();
    event->m_deviceId = nx::Uuid::createUuid();
    event->m_deviceIds = {nx::Uuid::createUuid(), nx::Uuid::createUuid()};
    const auto aggEvent = AggregatedEventPtr::create(event);

    TargetDeviceField field{&kDummyDescriptor};
    field.setUseSource(true);

    auto value = field.build(aggEvent);
    ASSERT_TRUE(value.isValid());
    ASSERT_FALSE(value.isNull());
    ASSERT_EQ(value.value<nx::Uuid>(), event->m_deviceIds.last());

    field.setUseSource(false);
    field.setId(nx::Uuid::createUuid());

    value = field.build(aggEvent);
    ASSERT_TRUE(value.isValid());
    ASSERT_FALSE(value.isNull());
    ASSERT_EQ(value.value<nx::Uuid>(), field.id());
}

void containsParametersForAllEvents(
    const utils::EventParameterHelper::EventParametersNames& visibleElements)
{
    ASSERT_TRUE(visibleElements.contains("site.name"));
    ASSERT_TRUE(visibleElements.contains("server.name"));
    ASSERT_TRUE(visibleElements.contains("device.name"));
}

void notContainsHiddenElements(
    const utils::EventParameterHelper::EventParametersNames& visibleElements)
{
    ASSERT_FALSE(visibleElements.contains("@CreateGuid"));
    ASSERT_FALSE(visibleElements.contains("event.eventName"));
    ASSERT_FALSE(visibleElements.contains("camera.id"));
}

TEST_F(ActionFieldTest, EventParametersHelperVisibleValuesAnyEvent)
{
    auto visibleElements = utils::EventParameterHelper::instance()->getVisibleEventParameters(
        utils::type<TestEvent>(), systemContext(), {});
    ASSERT_FALSE(visibleElements.empty());
    // List doesn't have parameters for soft trigger.
    ASSERT_FALSE(visibleElements.contains("user.name"));
    // List has element related to generic or analytics event.
    ASSERT_TRUE(visibleElements.contains("event.caption"));
    // List has element related to generic or analytics event or analytic object event.
    ASSERT_TRUE(visibleElements.contains("event.type"));

    containsParametersForAllEvents(visibleElements);
    notContainsHiddenElements(visibleElements);
}

TEST_F(ActionFieldTest, EventParametersHelperVisibleValuesForSoftTrigger)
{
    makeSoftTriggerEvent();

    auto visibleElements = utils::EventParameterHelper::instance()->getVisibleEventParameters(
        utils::type<SoftTriggerEvent>(), systemContext(), {});
    ASSERT_FALSE(visibleElements.empty());
    // List has parameters for soft trigger.
    ASSERT_TRUE(visibleElements.contains("user.name"));

    containsParametersForAllEvents(visibleElements);
    notContainsHiddenElements(visibleElements);
}

TEST_F(ActionFieldTest, EventParametersHelperVisibleValuesForDeviceEvents)
{
    auto visibleElements = utils::EventParameterHelper::instance()->getVisibleEventParameters(
        utils::type<TestEvent>(), systemContext(), {});
    ASSERT_FALSE(visibleElements.empty());

    // List has element related to device events.
    EXPECT_TRUE(visibleElements.contains("device.ip"));
    EXPECT_TRUE(visibleElements.contains("device.mac"));

    containsParametersForAllEvents(visibleElements);
    notContainsHiddenElements(visibleElements);
}

TEST_F(ActionFieldTest, EventParametersHelperUnregisteredEvent)
{
    auto visibleElements = utils::EventParameterHelper::instance()->getVisibleEventParameters(
        "customEventType", systemContext(), {});
    ASSERT_TRUE(visibleElements.empty());
}

TEST_F(ActionFieldTest, EventParametersProlongedEvents)
{
    makeAnalyticsEvent();

    auto visibleElements = utils::EventParameterHelper::instance()->getVisibleEventParameters(
        utils::type<AnalyticsEvent>(), systemContext(), {}, State::started);
    ASSERT_FALSE(visibleElements.empty());
    containsParametersForAllEvents(visibleElements);
    notContainsHiddenElements(visibleElements);

    ASSERT_TRUE(visibleElements.contains("event.time.start"));
    ASSERT_TRUE(visibleElements.contains("event.time.end"));
}

TEST_F(ActionFieldTest, EventParametersInstantEvents)
{
    makeAnalyticsEvent();

    auto visibleElements = utils::EventParameterHelper::instance()->getVisibleEventParameters(
        utils::type<AnalyticsEvent>(), systemContext(), {}, State::instant);
    ASSERT_FALSE(visibleElements.empty());
    containsParametersForAllEvents(visibleElements);
    notContainsHiddenElements(visibleElements);

    ASSERT_FALSE(visibleElements.contains("event.time.start"));
    ASSERT_FALSE(visibleElements.contains("event.time.end"));
}

} // namespace nx::vms::rules::test
