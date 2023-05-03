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
    TypeParam& object = this->object;
    // Checks if display offset setter/getter works correctly.
    object.setDisplayOffset(calendar_utils::kMinDisplayOffset);
    ASSERT_EQ(calendar_utils::kMinDisplayOffset, object.displayOffset());

    // Checks if minimum display offset value is constrained.
    object.setDisplayOffset(calendar_utils::kMinDisplayOffset - 1);
    ASSERT_EQ(calendar_utils::kMinDisplayOffset, object.displayOffset());

    // Checks if maximum display offset value is constrained.
    object.setDisplayOffset(calendar_utils::kMaxDisplayOffset + 1);
    ASSERT_EQ(calendar_utils::kMaxDisplayOffset, object.displayOffset());
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CommonTimePropertiesTest);
REGISTER_TYPED_TEST_SUITE_P(CommonTimePropertiesTest, DisplayOffsetPropertyCheck);

} // namespace test
} // namespace nx::vms::client::core
