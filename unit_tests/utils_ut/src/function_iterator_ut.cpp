#include <gtest/gtest.h>

#include <nx/utils/function_iterator.h>
#include <nx/utils/log/to_string.h>

namespace nx {
namespace utils {
namespace test {

TEST(FunctionIterator, Simple)
{
    std::vector<int> copyData;
    const auto pushBack = [&](int i) { copyData.push_back(i); };

    const std::vector<int> kTestData = {1, 2, 3};
    std::copy(kTestData.begin(), kTestData.end(), functionIterator<int>(pushBack));
    ASSERT_EQ(containerString(kTestData), containerString(copyData));
}

} // namespace test
} // namespace utils
} // namespace nx

