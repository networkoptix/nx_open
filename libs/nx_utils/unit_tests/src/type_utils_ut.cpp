#include <gtest/gtest.h>

#include <nx/utils/type_utils.h>

namespace nx::utils::test {

class TypeUtils:
    public ::testing::Test
{
};

TEST_F(TypeUtils, tuple_for_each_elements_are_iterated_forward)
{
    const auto t = std::make_tuple(1, 2, 3);
    std::vector<int> values;

    tuple_for_each(t, [&values](int value) { values.push_back(value); });

    ASSERT_EQ(std::vector<int>({1, 2, 3}), values);
}

TEST_F(TypeUtils, apply_each_empty_argument_list_is_supported)
{
    bool invoked = false;
    apply_each([&invoked]() { invoked = true; });
    ASSERT_FALSE(invoked);
}

TEST_F(TypeUtils, apply_each_elements_are_iterated_forward)
{
    std::vector<int> values;
    apply_each(
        [&values](int value) { values.push_back(value); },
        1, 2, 3);

    ASSERT_EQ(std::vector<int>({1, 2, 3}), values);
}

} // namespace nx::utils::test
