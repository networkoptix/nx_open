// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/log/format.h>
#include <nx/vms/rules/event_fields/builtin_fields.h>

namespace nx::vms::rules::test {

namespace {

template<class Field>
void testSimpleTypeField(
    const std::vector<typename Field::value_type> values,
    bool matchAny = false)
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

        if (matchAny)
        {
            EXPECT_TRUE(field.match(QVariant()));
            EXPECT_TRUE(field.match(QVariant::fromValue(ValueType())));
            EXPECT_TRUE(field.match(QVariant::fromValue(value)));
        }

        field.setValue(value);
        EXPECT_EQ(field.value(), value);

        EXPECT_TRUE(field.match(QVariant::fromValue(value)));
    }
}

} // namespace

TEST(EventFieldTest, SimpleTypes)
{
    testSimpleTypeField<ExpectedUuidField>({QnUuid(), QnUuid::createUuid()});
    testSimpleTypeField<IntField>({-1, 0, 60, 300, 86400});
    testSimpleTypeField<EventTextField>({ "", "Hello", "\\/!@#$%^&*()_+" }, true);
}

TEST(EventFieldTest, KeywordsField)
{
    static const struct {
        QString input;
        QString keywords;
        bool match = false;
    } testData[] = {
        // Empty keywords match any input.
        {"", "", true},
        {"baz", "", true},
        // Empty input do not match valid keywords.
        {"", "baz", false},
        // Valid keywords match input.
        {"baz", "baz", true},
        {"fizz baz bar", "baz", true},
        // Any keyword match is enough.
        {"fizz baz bar", "baz fizz", true},
        {"fizz bar", "baz fizz", true},
        {"fizz", "baz", false},
        // Keywords are trimmed and unquoted before match, quoted keywords may contain space.
        {"fizz", "baz   \"fizz\"", true},
        {"fizz baz bar", "\"baz bar\"", true},
        {"fizz baz  bar", "\"baz bar\"", false},
        {"fizz bar baz", "\"baz bar\"", false},
        // Keywords are first trimmed and then unqouted, not vice versa.
        {"fizz", "baz \" fizz\"", false},
        {"fizz", "baz \"fizz \"", false},
    };

    for(const auto& data: testData)
    {
        KeywordsField field;
        field.setString(data.keywords);

        SCOPED_TRACE(nx::format("Input: %1, keywords: %2", data.input, data.keywords).toStdString());
        EXPECT_EQ(field.match(data.input), data.match);
    }
}

} // namespace nx::vms::rules::test
