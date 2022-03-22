// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/rules/action_fields/builtin_fields.h>
#include <nx/vms/rules/engine.h>

#include "test_event.h"
#include "test_router.h"

namespace nx::vms::rules::test {

namespace {

template<class Field>
void testSimpleTypeField(const std::vector<typename Field::value_type> values)
{
    using ValueType = typename Field::value_type;

    for (const auto& value: values)
    {
        Field field;
        SCOPED_TRACE(nx::format(
            "Field type: %1, value: %2",
            field.metatype(),
            value).toStdString());

        EXPECT_EQ(field.value(), ValueType());

        field.setValue(value);
        EXPECT_EQ(field.value(), value);

        nx::vms::rules::EventData eventData;
        QVariant result = field.build(eventData);
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
        {"int", 123},
        {"string", "hello"},
        {"bool", true},
        {"float", 123.123},
    };

    std::unique_ptr<Engine> engine;

    TextWithFieldsTest()
        :engine(new Engine(std::make_unique<TestRouter>()))
    {
        engine->registerEvent(TestEvent::manifest());
    }
};

TEST_F(TextWithFieldsTest, CreateGuid)
{
    TextWithFields field;
    field.setText("{@CreateGuid}");
    EXPECT_TRUE(QnUuid::isUuidString(field.build(kEventData).toString()));
}

TEST_F(TextWithFieldsTest, EventType)
{
    TextWithFields field;
    field.setText("{@EventType}");
    EXPECT_EQ(TestEvent::manifest().id, field.build(kEventData).toString());
}

TEST_F(TextWithFieldsTest, EventName)
{
    TextWithFields field;
    field.setText("{@EventName}");
    EXPECT_EQ(TestEvent::manifest().displayName, field.build(kEventData).toString());
}

TEST_F(TextWithFieldsTest, EventCaption)
{
    TextWithFields field;
    auto eventData = kEventData;

    field.setText("{@EventCaption}");
    EXPECT_EQ(TestEvent::manifest().displayName, field.build(eventData).toString());

    constexpr auto kEventCaption = "Test caption";
    eventData.insert("caption", kEventCaption);
    EXPECT_EQ(kEventCaption, field.build(eventData).toString());
}

TEST_F(TextWithFieldsTest, EventDescription)
{
    TextWithFields field;
    auto eventData = kEventData;

    field.setText("{@EventDescription}");
    EXPECT_EQ(TestEvent::manifest().description, field.build(eventData).toString());

    constexpr auto kEventDescription = "Test description override";
    eventData.insert("description", kEventDescription);
    EXPECT_EQ(kEventDescription, field.build(eventData).toString());
}

TEST_P(TextWithFieldsTest, FormatLine)
{
    const auto [expected, format] = GetParam();

    TextWithFields field;
    field.setText(format);
    SCOPED_TRACE(nx::format("Format line: %1", field.text()).toStdString());

    auto result = field.build(kEventData);
    ASSERT_TRUE(result.isValid());

    EXPECT_EQ(expected.toStdString(), result.toString().toStdString());
}

static const std::vector<FormatResult> kFormatResults
{
    // Empty format string.
    {"", ""},

    // Test valid formatting
    {"int 123", "int {int}"},
    {"int {} int", "int {} int"},
    {"string hello", "string {string}"},
    {"bool true", "bool {bool}"},
    {"float 123.123", "float {float}"},
    {"123 hello true 123.123", "{int} {string} {bool} {float}"},

    // Test placeholders.
    {"123 {absent}", "{int} {absent}"},

    // Test invalid format string.
    {"int {int", "int {int"},
    {"int} int", "int} int"},
    {"int {} int", "int {} int"},

    // Test exotic cases.
    {"{{123}}", "{{{int}}}"},
    {"}{{123}}", "}{{{int}}}"},
    {"{{123}}{", "{{{int}}}{"},
    {"{ { 123}}", "{ { {int}}}"},
    {"{ 123}}", "{ {int}}}"},
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
