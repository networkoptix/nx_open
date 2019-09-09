#include <gtest/gtest.h>

#include <nx/client/core/time/display_time_helper.h>
#include <nx/client/core/time/time_common_properties_test.h>

namespace nx::client::core {
namespace test {

using TestingType = testing::Types<DisplayTimeHelper>;
INSTANTIATE_TYPED_TEST_CASE_P(DisplayTimeHelperTest, CommonTimePropertiesTest, TestingType);

} // namespace test
} // namespace nx::client::core
