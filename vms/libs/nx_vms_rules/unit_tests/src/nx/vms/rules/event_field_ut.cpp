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

} // namespace nx::vms::rules::test
