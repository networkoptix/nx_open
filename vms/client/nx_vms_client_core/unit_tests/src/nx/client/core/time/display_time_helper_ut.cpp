#include <gtest/gtest.h>

#include <nx/client/core/time/display_time_helper.h>
#include <nx/client/core/time/time_common_properties_test_helpers.h>

namespace {

using Helper = nx::client::core::DisplayTimeHelper;
using HelperPtr = QSharedPointer<Helper>;

HelperPtr createHelper()
{
    return HelperPtr(new Helper());
}

} // namespace

TEST(DisplayTimeHelperTest, DisplayOffsetPropertyCheck)
{
    checkDisplayOffsetPropertyInteractionAndRanges(createHelper().get());
}

TEST(DisplayTimeHelperTest, PositionPropertyCheck)
{
    checkPositionPropertyInteractionAndRanges(createHelper().get());
}


