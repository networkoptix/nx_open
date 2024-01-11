// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>

#include <gtest/gtest.h>

#include <QtCore/QString>

#include <nx/vms/common/test_support/test_context.h>
#include <nx/vms/rules/action_builder_fields/builtin_fields.h>
#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter_fields/builtin_fields.h>
#include <nx/vms/rules/events/builtin_events.h>
#include <nx/vms/rules/utils/event_parameter_helper.h>

#include "test_event.h"
#include "test_router.h"

namespace nx::vms::rules::test {

namespace {

template<class Field, class T>
void testSimpleTypeField(const std::initializer_list<T>& values)
{
    using ValueType = typename Field::value_type;

    for (const auto& testValue: values)
    {
        Field field;
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
    public common::test::ContextBasedTest,
    public TestEngineHolder,
    public ::testing::WithParamInterface<FormatResult>
{
public:
    const EventData kEventData = {
        {"type", TestEvent::manifest().id},
        {"intField", 123},
        {"text", "hello"},
        {"boolField", true},
        {"floatField", 123.123},
    };
    const QString kAttributeNameField = QStringLiteral("Name");
    const QString kAttributeName = QStringLiteral("Temporal Displacement");
    const QString kAttributeLicensePlateNumberField = QStringLiteral("LicensePlate.Number");
    const QString kAttributeLicensePlateNumber = QStringLiteral("OUTATIME");

    ActionFieldTest(): TestEngineHolder(context()->systemContext())
    {
        engine->registerEvent(TestEvent::manifest(), []{ return new TestEvent(); });
    }

    EventPtr makeEvent(State state = State::instant)
    {
        auto event = engine->buildEvent(kEventData);
        auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch());
        event->setTimestamp(timestamp);
        event->setState(state);
        return event;
    }

    EventPtr makeAnalyticsObjectEvent()
    {
        engine->registerEventField(
            fieldMetatype<SourceCameraField>(), [] { return new SourceCameraField(); });
        engine->registerEventField(fieldMetatype<AnalyticsObjectTypeField>(),
            [this] { return new AnalyticsObjectTypeField(systemContext()); });
        engine->registerEventField(fieldMetatype<ObjectLookupField>(),
            [this] { return new ObjectLookupField(systemContext()); });
        engine->registerEvent(
            AnalyticsObjectEvent::manifest(), [] { return new AnalyticsObjectEvent(); });

        static const EventData kEventAtributeData = {
            {"type", AnalyticsObjectEvent::manifest().id},
            {"objectTypeId", "nx.base.Vehicle"}
        };
        auto event = engine->buildEvent(kEventAtributeData);

        // This has to be added manually since the Json conversion will fail.
        static const nx::common::metadata::Attributes kAttributes{
            {kAttributeNameField, kAttributeName},
            {kAttributeLicensePlateNumberField, kAttributeLicensePlateNumber}};

        event->setProperty("attributes", QVariant::fromValue(kAttributes));
        return event;
    }

    EventPtr makeAnalyticsEvent()
    {
        engine->registerEventField(fieldMetatype<StateField>(), [] { return new StateField(); });
        engine->registerEventField(
            fieldMetatype<SourceCameraField>(), [] { return new SourceCameraField(); });
        engine->registerEventField(fieldMetatype<AnalyticsEventTypeField>(),
            [this] { return new AnalyticsEventTypeField(systemContext()); });
        engine->registerEventField(fieldMetatype<TextLookupField>(),
            [this] { return new TextLookupField(systemContext()); });
        engine->registerEventField(fieldMetatype<TextLookupField>(),
            [this] { return new TextLookupField(systemContext()); });
        engine->registerEvent(AnalyticsEvent::manifest(), [] { return new AnalyticsEvent(); });

        static const EventData kEventAtributeData = {
            {"type", AnalyticsEvent::manifest().id}
        };
        return engine->buildEvent(kEventAtributeData);
    }

    EventPtr makeSoftTriggerEvent()
    {
        engine->registerEventField(fieldMetatype<UniqueIdField>(), [] { return new UniqueIdField(); });
        engine->registerEventField(
            fieldMetatype<SourceCameraField>(), [] { return new SourceCameraField(); });
        engine->registerEventField(fieldMetatype<SourceUserField>(),
            [this] { return new SourceUserField(systemContext()); });
        engine->registerEventField(fieldMetatype<CustomizableTextField>(),
            [this] { return new CustomizableTextField(); });
        engine->registerEventField(fieldMetatype<CustomizableIconField>(),
            [this] { return new CustomizableIconField(); });
        engine->registerEvent(SoftTriggerEvent::manifest(), [] { return new SoftTriggerEvent(); });

        static const EventData kEventAtributeData = {
            {"type", SoftTriggerEvent::manifest().id},
        };
        return engine->buildEvent(kEventAtributeData);
    }

};

TEST_F(ActionFieldTest, EventTimeInstantEvent)
{
    TextWithFields field(systemContext());
    field.setText("{event.time}");
    auto event = AggregatedEventPtr::create(makeEvent());
    EXPECT_EQ(nx::utils::timestampToISO8601(event->timestamp()), field.build(event).toString());
}

TEST_F(ActionFieldTest, EventTimestamp)
{
    TextWithFields field(systemContext());
    field.setText("{event.timestamp}");
    auto event = AggregatedEventPtr::create(makeEvent());

    QDateTime time;
    time = time.addMSecs(
        std::chrono::duration_cast<std::chrono::milliseconds>(event->timestamp()).count());

    EXPECT_EQ(time.toString(), field.build(event).toString());
}

TEST_F(ActionFieldTest, EventStartTimeInstantEvent)
{
    TextWithFields fieldStartTime(systemContext());
    QString arg = "{event.time.start}";
    fieldStartTime.setText(arg);
    // There is no start time for Instant event, so there is no substitution.
    EXPECT_EQ(arg, fieldStartTime.build(AggregatedEventPtr::create(makeEvent())).toString());
}

TEST_F(ActionFieldTest, EventTimeProlongedEvent)
{
    TextWithFields fieldActiveStartTime(systemContext());
    fieldActiveStartTime.setText("{event.time.start}");

    TextWithFields fieldActiveTime(systemContext());
    fieldActiveTime.setText("{event.time}");

    TextWithFields fieldActiveEndTime(systemContext());
    fieldActiveEndTime.setText("{event.time.end}");

    auto eventStart = makeEvent(State::started);
    engine->processEvent(eventStart);

    auto aggregatedEvent = AggregatedEventPtr::create(eventStart);
    auto activeEventTime = fieldActiveTime.build(aggregatedEvent).toString();

    // Event start time and timestamp are equal.
    EXPECT_EQ(activeEventTime, fieldActiveStartTime.build(aggregatedEvent).toString());

    // Event doesn't have end time yet.
    EXPECT_TRUE(fieldActiveEndTime.build(aggregatedEvent).toString().isEmpty());

    auto eventEnd = makeEvent(State::stopped);
    engine->processEvent(eventEnd);

    TextWithFields fieldInactiveStartTime(systemContext());
    fieldInactiveStartTime.setText("{event.time.start}");

    TextWithFields fieldInactiveTime(systemContext());
    fieldInactiveTime.setText("{event.time}");

    TextWithFields fieldInactiveEndTime(systemContext());
    fieldInactiveEndTime.setText("{event.time.end}");

    aggregatedEvent = AggregatedEventPtr::create(eventEnd);

    // Start time is start of previous event.
    EXPECT_EQ(activeEventTime, fieldInactiveStartTime.build(aggregatedEvent).toString());

    // End time is time of current event.
    EXPECT_EQ(fieldInactiveTime.build(aggregatedEvent).toString(),
        fieldInactiveEndTime.build(aggregatedEvent).toString());
}

TEST_F(ActionFieldTest, EventAttributes)
{
    TextWithFields field(systemContext());
    auto event = makeAnalyticsObjectEvent();
    auto aggregatedEvent = AggregatedEventPtr::create(event);
    field.setText(
        QStringLiteral("{event.attributes.%1} {event.attributes.%2} \"{event.attributes.empty}\"")
            .arg(kAttributeNameField)
            .arg(kAttributeLicensePlateNumberField));

    const auto& actual = field.build(aggregatedEvent).toString();
    const auto& expected = QStringLiteral("%1 %2 \"\"").arg(kAttributeName).arg(kAttributeLicensePlateNumber);

    EXPECT_EQ(expected, actual);
}

TEST_F(ActionFieldTest, EventDotType)
{
    TextWithFields field(systemContext());
    field.setText("{event.type}");
    EXPECT_EQ(
        AnalyticsEvent::manifest().id,
        field.build(AggregatedEventPtr::create(makeAnalyticsEvent())).toString());
}

TEST_F(ActionFieldTest, EventDotName)
{
    TextWithFields field(systemContext());
    field.setText("{event.name}");
    EXPECT_EQ(
        TestEvent::manifest().displayName,
        field.build(AggregatedEventPtr::create(makeEvent())).toString());
}

TEST_F(ActionFieldTest, EventDotNameAnalyticsEvent)
{
    TextWithFields field(systemContext());
    field.setText("{event.name}");
    EXPECT_EQ(
        AnalyticsEvent::manifest().displayName,
        field.build(AggregatedEventPtr::create(makeAnalyticsEvent())).toString());
}

TEST_F(ActionFieldTest, EventCaption)
{
    auto event = makeAnalyticsEvent();
    TextWithFields field(systemContext());
    auto eventAggregator = AggregatedEventPtr::create(event);

    field.setText("{@EventCaption}");
    EXPECT_EQ(AnalyticsEvent::manifest().displayName, field.build(eventAggregator).toString());

    constexpr auto kEventCaption = "Test caption";
    event->setProperty("caption", kEventCaption);

    EXPECT_EQ(kEventCaption, field.build(eventAggregator).toString());
}

TEST_F(ActionFieldTest, EventDotCaption)
{
    auto event = makeAnalyticsEvent();
    TextWithFields field(systemContext());
    auto eventAggregator = AggregatedEventPtr::create(event);

    field.setText("{event.caption}");
    EXPECT_EQ(AnalyticsEvent::manifest().displayName, field.build(eventAggregator).toString());

    constexpr auto kEventCaption = "Test caption";
    event->setProperty("caption", kEventCaption);

    EXPECT_EQ(kEventCaption, field.build(eventAggregator).toString());
}

TEST_F(ActionFieldTest, EventDescription)
{
    TextWithFields field(systemContext());
    auto event = makeEvent();
    auto eventAggregator = AggregatedEventPtr::create(event);

    field.setText("{@EventDescription}");
    EXPECT_EQ(TestEvent::manifest().description, field.build(eventAggregator).toString());

    constexpr auto kEventDescription = "Test description override";
    event->setProperty("description", kEventDescription);

    EXPECT_EQ(kEventDescription, field.build(eventAggregator).toString());
}

TEST_F(ActionFieldTest, EventDotDescription)
{
    TextWithFields field(systemContext());
    auto event = makeEvent();
    auto eventAggregator = AggregatedEventPtr::create(event);

    field.setText("{event.description}");
    EXPECT_EQ(TestEvent::manifest().description, field.build(eventAggregator).toString());

    constexpr auto kEventDescription = "Test description override";
    event->setProperty("description", kEventDescription);

    EXPECT_EQ(kEventDescription, field.build(eventAggregator).toString());
}

TEST_F(ActionFieldTest, EventDotSource)
{
    TextWithFields field(systemContext());
    auto event = makeEvent();
    auto eventAggregator = AggregatedEventPtr::create(event);

    field.setText("{event.source}");
    static const auto kZeroId = u"{00000000-0000-0000-0000-000000000000}";

    EXPECT_EQ(kZeroId, field.build(eventAggregator).toString());
}

TEST_F(ActionFieldTest, EventTooltip)
{
    TextWithFields field(systemContext());

    field.setText("{@ExtendedEventDescription}");

    const auto tooltip = field.build(AggregatedEventPtr::create(makeEvent())).toString();
    EXPECT_NE(tooltip, field.text());
    // Tooltip must not be empty, at least event name and timestamp must exists.
    EXPECT_FALSE(tooltip.isEmpty());
}

TEST_P(ActionFieldTest, FormatLine)
{
    const auto [expected, format] = GetParam();

    TextWithFields field(systemContext());
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
    {"int 123", "int {intField}"},
    {"int {} int", "int {} int"},
    {"string hello", "string {text}"},
    {"bool true", "bool {boolField}"},
    {"float 123.123", "float {floatField}"},
    {"123 hello true 123.123", "{intField} {text} {boolField} {floatField}"},

    // Test placeholders.
    {"123 {absent}", "{intField} {absent}"},

    // Test invalid format string.
    {"int {int", "int {int"},
    {"int} int", "int} int"},
    {"int {} int", "int {} int"},

    // Test exotic cases.
    {"{{123}}", "{{{intField}}}"},
    {"}{{123}}", "}{{{intField}}}"},
    {"{{123}}{", "{{{intField}}}{"},
    {"{ { 123}}", "{ { {intField}}}"},
    {"{ 123}}", "{ {intField}}}"},
    {"{ {int{}}}", "{ {int{}}}"},
};

INSTANTIATE_TEST_SUITE_P(CommonSet,
    ActionFieldTest,
    ::testing::ValuesIn(kFormatResults));


TEST_F(ActionFieldTest, TargetUserField)
{
    UuidSelection selection;
    selection.ids << QnUuid::createUuid() << QnUuid::createUuid();
    selection.all = true;

    TargetUserField field(systemContext());
    field.setIds(selection.ids);
    field.setAcceptAll(selection.all);

    EXPECT_EQ(selection.ids, field.ids());
    EXPECT_EQ(selection.all, field.acceptAll());

    auto result = field.build(/*eventData*/ {});
    EXPECT_TRUE(result.canConvert<UuidSelection>());

    EXPECT_EQ(selection, result.value<UuidSelection>());
}

TEST_F(ActionFieldTest, EventIdField)
{
    const auto event = QSharedPointer<TestEvent>::create();
    const auto aggEvent = AggregatedEventPtr::create(event);

    const auto field = QSharedPointer<EventIdField>::create();
    const auto value = field->build(aggEvent);

    ASSERT_FALSE(aggEvent->id().isNull());
    ASSERT_TRUE(value.isValid());
    ASSERT_FALSE(value.isNull());
    ASSERT_EQ(value.value<QnUuid>(), aggEvent->id());
}

TEST_F(ActionFieldTest, TargetSingleDeviceTest)
{
    const auto event = QSharedPointer<TestEvent>::create();
    event->m_cameraId = QnUuid::createUuid();
    event->m_deviceIds = {QnUuid::createUuid(), QnUuid::createUuid()};
    const auto aggEvent = AggregatedEventPtr::create(event);

    TargetSingleDeviceField field;
    field.setUseSource(true);

    auto value = field.build(aggEvent);
    ASSERT_TRUE(value.isValid());
    ASSERT_FALSE(value.isNull());
    ASSERT_EQ(value.value<QnUuid>(), event->m_deviceIds.last());

    field.setUseSource(false);
    field.setId(QnUuid::createUuid());

    value = field.build(aggEvent);
    ASSERT_TRUE(value.isValid());
    ASSERT_FALSE(value.isNull());
    ASSERT_EQ(value.value<QnUuid>(), field.id());
}

void containsParametersForAllEvents(
    const utils::EventParameterHelper::EventParametersNames& visibleElements)
{
    ASSERT_TRUE(visibleElements.contains("system.name"));
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

TEST_F(ActionFieldTest, EventParametersHelperVisibleValuesForGeneric)
{
    auto visibleElements = utils::EventParameterHelper::getVisibleEventParameters(
        utils::type<GenericEvent>(), systemContext(), {});
    ASSERT_FALSE(visibleElements.empty());
    // List dont have parameters for soft trigger.
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

    auto visibleElements = utils::EventParameterHelper::getVisibleEventParameters(
        utils::type<SoftTriggerEvent>(), systemContext(), {});
    ASSERT_FALSE(visibleElements.empty());
    // List has parameters for soft trigger.
    ASSERT_TRUE(visibleElements.contains("user.name"));

    containsParametersForAllEvents(visibleElements);
    notContainsHiddenElements(visibleElements);
}

TEST_F(ActionFieldTest, EventParametersHelperVisibleValuesForDeviceEvents)
{
    // Required for registering the event.
    makeAnalyticsObjectEvent();

    auto visibleElements = utils::EventParameterHelper::getVisibleEventParameters(
        utils::type<AnalyticsObjectEvent>(), systemContext(), {});
    ASSERT_FALSE(visibleElements.empty());

    // List has element related to device events.
    ASSERT_TRUE(visibleElements.contains("device.ip"));
    ASSERT_TRUE(visibleElements.contains("device.mac"));

    containsParametersForAllEvents(visibleElements);
    notContainsHiddenElements(visibleElements);
}

TEST_F(ActionFieldTest, EventParametersHelperCustomEvent)
{
    auto visibleElements = utils::EventParameterHelper::getVisibleEventParameters(
        "customEventType", systemContext(), {});
    ASSERT_FALSE(visibleElements.empty());
    containsParametersForAllEvents(visibleElements);
    notContainsHiddenElements(visibleElements);
}

TEST_F(ActionFieldTest, EventParametersProlongedEvents)
{
    makeAnalyticsEvent();

    auto visibleElements = utils::EventParameterHelper::getVisibleEventParameters(
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

    auto visibleElements = utils::EventParameterHelper::getVisibleEventParameters(
        utils::type<AnalyticsEvent>(), systemContext(), {}, State::instant);
    ASSERT_FALSE(visibleElements.empty());
    containsParametersForAllEvents(visibleElements);
    notContainsHiddenElements(visibleElements);

    ASSERT_FALSE(visibleElements.contains("event.time.start"));
    ASSERT_FALSE(visibleElements.contains("event.time.end"));
}

} // namespace nx::vms::rules::test
