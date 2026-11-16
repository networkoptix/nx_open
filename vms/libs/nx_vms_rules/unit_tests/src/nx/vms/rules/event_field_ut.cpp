// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <vector>

#include <gtest/gtest.h>

#include <nx/utils/log/format.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx/vms/rules/event_filter_fields/builtin_fields.h>
#include <nx/vms/rules/manifest.h>

namespace nx::vms::rules::test {

namespace {

const FieldDescriptor kDummyDescriptor;

template<class Field>
void testSimpleTypeField(
    const std::vector<typename Field::value_type> values,
    bool matchAny = false)
{
    using ValueType = typename Field::value_type;

    for (const auto& value: values)
    {
        Field field{&kDummyDescriptor};
        SCOPED_TRACE(nx::format(
            "Field type: %1, value: %2",
            field.type(),
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

        EXPECT_TRUE(field.match(QVariant::fromValue<ValueType>(value)));
    }
}

} // namespace

class EventFieldContextTest: public nx::vms::common::test::ContextBasedTest
{
};

TEST_F(EventFieldContextTest, SourceUserField)
{
    const auto group = createUserGroup(
        "User input group", NoGroup, {{api::kAllDevicesGroupId, api::AccessRight::userInput}});
    const auto user = addUser({group.id});
    const auto userIdValue = QVariant::fromValue(user->getId());

    auto field = SourceUserField(systemContext(), &kDummyDescriptor);

    // Default field do not match anything.
    EXPECT_FALSE(field.match(userIdValue));

    // Accept all match any user.
    field.setAcceptAll(true);
    EXPECT_TRUE(field.match(userIdValue));
    EXPECT_TRUE(field.match({}));
    field.setAcceptAll(false);

    // User id is matched.
    field.setIds({user->getId()});
    EXPECT_TRUE(field.match(userIdValue));

    // User role is matched.
    field.setIds({group.id});
    EXPECT_TRUE(field.match(userIdValue));
}

TEST(EventFieldTest, SimpleTypes)
{
    testSimpleTypeField<EventFlagField>({true, false});
    testSimpleTypeField<ExpectedUuidField>({nx::Uuid(), nx::Uuid::createUuid()});
    testSimpleTypeField<IntField>({-1, 0, 60, 300, 86400});
    testSimpleTypeField<EventTextField>({ "", "Hello", "\\/!@#$%^&*()_+" }, true);
}

} // namespace nx::vms::rules::test
