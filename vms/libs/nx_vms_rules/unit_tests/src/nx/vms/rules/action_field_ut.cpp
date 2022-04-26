// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/rules/action_fields/builtin_fields.h>
#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/engine.h>

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

TEST(ActionFieldTest, SimpleTypes)
{
    testSimpleTypeField<FlagField>({true, false});
    testSimpleTypeField<OptionalTimeField>({-1, 0, 60, 300, 86400});
    testSimpleTypeField<ActionTextField>({ "", "Hello", "\\/!@#$%^&*()_+" });
}

struct FormatResult
{
    QString expected;
    QString format;
};

class TextWithFieldsTest:
    public ::testing::Test,
    public ::testing::WithParamInterface<FormatResult>
{
public:
    const EventData kEventData = {
        {"type", TestEvent::manifest().id},
        {"intField", 123},
        {"stringField", "hello"},
        {"boolField", true},
        {"floatField", 123.123},
    };

    std::unique_ptr<Engine> engine;

    TextWithFieldsTest()
        :engine(new Engine(std::make_unique<TestRouter>()))
    {
        engine->registerEvent(TestEvent::manifest(), []{ return new TestEvent(); });
    }

    AggregatedEvent makeAggregatedEvent()
    {
        AggregatedEvent aggregatedEvent;
        aggregatedEvent.aggregate(engine->buildEvent(kEventData));
        return aggregatedEvent;
    }
};

TEST_F(TextWithFieldsTest, CreateGuid)
{
    TextWithFields field;
    field.setText("{@CreateGuid}");
    EXPECT_TRUE(QnUuid::isUuidString(field.build(makeAggregatedEvent()).toString()));
}

TEST_F(TextWithFieldsTest, EventType)
{
    TextWithFields field;
    field.setText("{@EventType}");
    EXPECT_EQ(TestEvent::manifest().id, field.build(makeAggregatedEvent()).toString());
}

TEST_F(TextWithFieldsTest, EventName)
{
    TextWithFields field;
    field.setText("{@EventName}");
    EXPECT_EQ(TestEvent::manifest().displayName, field.build(makeAggregatedEvent()).toString());
}

TEST_F(TextWithFieldsTest, EventCaption)
{
    TextWithFields field;
    AggregatedEvent aggregatedEvent(makeAggregatedEvent());

    field.setText("{@EventCaption}");
    EXPECT_EQ(TestEvent::manifest().displayName, field.build(aggregatedEvent).toString());

    constexpr auto kEventCaption = "Test caption";
    aggregatedEvent.initialEvent()->setProperty("caption", kEventCaption);

    EXPECT_EQ(kEventCaption, field.build(aggregatedEvent).toString());
}

TEST_F(TextWithFieldsTest, EventDescription)
{
    TextWithFields field;
    AggregatedEvent aggregatedEvent(makeAggregatedEvent());

    field.setText("{@EventDescription}");
    EXPECT_EQ(TestEvent::manifest().description, field.build(aggregatedEvent).toString());

    constexpr auto kEventDescription = "Test description override";
    aggregatedEvent.initialEvent()->setProperty("description", kEventDescription);

    EXPECT_EQ(kEventDescription, field.build(aggregatedEvent).toString());
}

TEST_P(TextWithFieldsTest, FormatLine)
{
    const auto [expected, format] = GetParam();

    TextWithFields field;
    field.setText(format);
    SCOPED_TRACE(nx::format("Format line: %1", field.text()).toStdString());

    auto result = field.build(makeAggregatedEvent());
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
    {"string hello", "string {stringField}"},
    {"bool true", "bool {boolField}"},
    {"float 123.123", "float {floatField}"},
    {"123 hello true 123.123", "{intField} {stringField} {boolField} {floatField}"},

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
    TextWithFieldsTest,
    ::testing::ValuesIn(kFormatResults));


TEST(ActionFieldTest, TargetUserField)
{
    UuidSelection selection;
    selection.ids << QnUuid::createUuid() << QnUuid::createUuid();
    selection.all = true;

    TargetUserField field;
    field.setIds(selection.ids);
    field.setAcceptAll(selection.all);

    EXPECT_EQ(selection.ids, field.ids());
    EXPECT_EQ(selection.all, field.acceptAll());

    auto result = field.build(/*eventData*/ {});
    EXPECT_TRUE(result.canConvert<UuidSelection>());

    EXPECT_EQ(selection, result.value<UuidSelection>());
}

} // namespace nx::vms::rules::test
