// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QObject>

#include <nx/vms/client/core/time/calendar_utils.h>

namespace nx::vms::client::core {
namespace test {

template<typename Base>
struct CommonTimePropertiesTest: public testing::Test
{
    Base object;
};

TYPED_TEST_SUITE_P(CommonTimePropertiesTest);

TYPED_TEST_P(CommonTimePropertiesTest, DisplayOffsetPropertyCheck)
{
    using TimeConstants = nx::vms::client::core::CalendarUtils;

    TypeParam& object = this->object;
    // Checks if display offset setter/getter works correctly.
    object.setDisplayOffset(TimeConstants::kMinDisplayOffset);
    ASSERT_EQ(TimeConstants::kMinDisplayOffset, object.displayOffset());

    // Checks if minimum display offset value is constrained.
    object.setDisplayOffset(TimeConstants::kMinDisplayOffset - 1);
    ASSERT_EQ(TimeConstants::kMinDisplayOffset, object.displayOffset());

    // Checks if maximum display offset value is constrained.
    object.setDisplayOffset(TimeConstants::kMaxDisplayOffset + 1);
    ASSERT_EQ(TimeConstants::kMaxDisplayOffset, object.displayOffset());
}

TYPED_TEST_P(CommonTimePropertiesTest, PositionPropertyCheck)
{
    TypeParam& object = this->object;

    // Checks if current position setter/getter works correctly.
    static const int kSomePosition = 1000;
    object.setPosition(kSomePosition);
    ASSERT_EQ(kSomePosition, object.position());

    // Checks if minimum position value is constrained.
    object.setPosition(-1);
    ASSERT_EQ(object.position(), 0);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CommonTimePropertiesTest);
REGISTER_TYPED_TEST_SUITE_P(CommonTimePropertiesTest,
    DisplayOffsetPropertyCheck,
    PositionPropertyCheck);

} // namespace test
} // namespace nx::vms::client::core

