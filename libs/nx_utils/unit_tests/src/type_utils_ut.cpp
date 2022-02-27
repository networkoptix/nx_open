// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

TEST_F(TypeUtils, tuple_first_element)
{
    static_assert(std::is_same_v<
        int,
        tuple_first_element_t<std::tuple<int, std::string>, void>>);

    static_assert(std::is_same_v<
        std::string,
        tuple_first_element_t<std::tuple<>, std::string>>);
}

TEST_F(TypeUtils, tuple_last_element)
{
    static_assert(std::is_same_v<
        std::string,
        tuple_last_element_t<std::tuple<int, std::string>>>);

    static_assert(std::is_same_v<
        std::string,
        tuple_last_element_t<std::tuple<>, std::string>>);
}

TEST_F(TypeUtils, make_tuple_from_arg_range)
{
    const std::string test = "Hello, world";

    auto x = make_tuple_from_arg_range<1, 1>(1, test, std::vector<int>());
    ASSERT_EQ(std::make_tuple(test), x);

    x = make_tuple_from_arg_range<0, 1>(test);
    ASSERT_EQ(std::make_tuple(test), x);

    x = make_tuple_from_arg_range<1, 1>(1, test);
    ASSERT_EQ(std::make_tuple(test), x);
}

TEST_F(TypeUtils, make_tuple_from_arg_range_with_move_only_types)
{
    const std::string test = "Hello, world";

    auto moveOnly = std::make_unique<int>(1);

    const auto y = make_tuple_from_arg_range<0, 2>(std::move(moveOnly), test);
    ASSERT_EQ(1, *std::get<0>(y));
    ASSERT_EQ(test, std::get<1>(y));
}

TEST_F(TypeUtils, make_tuple_from_arg_range_out_of_range_arguments_are_not_modified)
{
    std::string x("x");
    std::string y("y");

    const auto z = make_tuple_from_arg_range<0, 1>(std::move(x), std::move(y));
    ASSERT_EQ("x", std::get<0>(z));
    ASSERT_EQ("y", y);
}

} // namespace nx::utils::test
