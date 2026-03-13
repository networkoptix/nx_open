// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <nx/ranges/views/cat_value.h>

namespace nx::ranges::test {
namespace {

using namespace std::literals;

std::map<std::string, std::unique_ptr<int>> makeUniquePtrMap(int a, int b)
{
    std::map<std::string, std::unique_ptr<int>> result;
    result.emplace("alpha", std::make_unique<int>(a));
    result.emplace("gamma", std::make_unique<int>(b));
    return result;
}

TEST(ranges, catValueLeftTupleLikeAdaptorLvalueRange)
{
    std::array values = std::to_array<std::pair<int, int>>({{1, 10}, {2, 20}});

    for (auto&& [tag, key, value]: values | nx::views::catLeft(100))
    {
        EXPECT_EQ(tag, 100);
        key += 1;
        value += tag;
    }

    EXPECT_EQ(values[0].first, 2);
    EXPECT_EQ(values[0].second, 110);
    EXPECT_EQ(values[1].first, 3);
    EXPECT_EQ(values[1].second, 120);
}

TEST(ranges, catValueRightScalarAdaptorLvalueRange)
{
    std::array values{1, 2, 3};

    for (auto&& [value, tag]: values | nx::views::catRight("rhs"sv))
    {
        EXPECT_EQ(tag, "rhs"sv);
        value *= 2;
    }

    EXPECT_EQ(values[0], 2);
    EXPECT_EQ(values[1], 4);
    EXPECT_EQ(values[2], 6);
}

TEST(ranges, catValueNonAdaptorOverloads)
{
    const std::array values{7, 8};

    {
        std::vector<std::tuple<std::string_view, int>> rows;
        for (const auto& [tag, value]: nx::views::catLeft(values, "left"sv))
            rows.emplace_back(tag, value);

        ASSERT_EQ(rows.size(), 2u);
        EXPECT_EQ(std::get<0>(rows[0]), "left"sv);
        EXPECT_EQ(std::get<1>(rows[0]), 7);
        EXPECT_EQ(std::get<0>(rows[1]), "left"sv);
        EXPECT_EQ(std::get<1>(rows[1]), 8);
    }

    {
        std::vector<std::tuple<int, std::string_view>> rows;
        for (const auto& [value, tag]: nx::views::catRight(values, "right"sv))
            rows.emplace_back(value, tag);

        ASSERT_EQ(rows.size(), 2u);
        EXPECT_EQ(std::get<0>(rows[0]), 7);
        EXPECT_EQ(std::get<1>(rows[0]), "right"sv);
        EXPECT_EQ(std::get<0>(rows[1]), 8);
        EXPECT_EQ(std::get<1>(rows[1]), "right"sv);
    }
}

TEST(ranges, catValueUnwrapsReferenceWrapper)
{
    const std::array values{3, 4};
    int bound = 10;

    for (const auto& [lhs, rhs]: values | nx::views::catLeft(std::ref(bound)))
        lhs += rhs;

    EXPECT_EQ(bound, 17);
}

TEST(ranges, catValueTupleLikeOwningAsRvalueWorksWithMapOfUniquePtr)
{
    {
        std::vector<std::tuple<std::string_view, std::string, int>> rows;

        for (auto&& [serverName, deviceName, idPtr]:
                makeUniquePtrMap(33, 44)
                | std::views::as_rvalue
                | nx::views::catLeft("beta"sv))
        {
            ASSERT_TRUE(idPtr);
            rows.emplace_back(serverName, deviceName, *idPtr);
        }

        ASSERT_EQ(rows.size(), 2u);
        EXPECT_EQ(std::get<0>(rows[0]), "beta"sv);
        EXPECT_EQ(std::get<1>(rows[0]), "alpha");
        EXPECT_EQ(std::get<2>(rows[0]), 33);
        EXPECT_EQ(std::get<0>(rows[1]), "beta"sv);
        EXPECT_EQ(std::get<1>(rows[1]), "gamma");
        EXPECT_EQ(std::get<2>(rows[1]), 44);
    }

    {
        std::vector<std::tuple<std::string, int, std::string_view>> rows;

        for (auto&& [deviceName, idPtr, serverName]:
                nx::views::catRight(
                    makeUniquePtrMap(55, 66) | std::views::as_rvalue,
                    "prod"sv))
        {
            ASSERT_TRUE(idPtr);
            rows.emplace_back(deviceName, *idPtr, serverName);
        }

        ASSERT_EQ(rows.size(), 2u);
        EXPECT_EQ(std::get<0>(rows[0]), "alpha");
        EXPECT_EQ(std::get<1>(rows[0]), 55);
        EXPECT_EQ(std::get<2>(rows[0]), "prod"sv);
        EXPECT_EQ(std::get<0>(rows[1]), "gamma");
        EXPECT_EQ(std::get<1>(rows[1]), 66);
        EXPECT_EQ(std::get<2>(rows[1]), "prod"sv);
    }
}

} // namespace
} // namespace nx::ranges::test
