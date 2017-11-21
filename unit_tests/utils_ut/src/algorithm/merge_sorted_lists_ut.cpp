#include <gtest/gtest.h>

#include <vector>

#include <nx/utils/algorithm/merge_sorted_lists.h>

namespace nx {
namespace utils {
namespace test {

class Vector: public std::vector<int>
{
    using base_type = std::vector<int>;

public:
    Vector(): base_type() {}
    Vector(Vector&& other): base_type(other) {}
    Vector(const std::initializer_list<int>& init): base_type(init) {}
    Vector(const Vector& other): base_type(other)
    {
        check_deep_copy();
    }

private:
    void check_deep_copy() const
    {
        const bool deep_copied = !empty();
        ASSERT_FALSE(deep_copied);
    }
};

TEST(MergeSortedLists, AscendingOrder)
{
    std::vector<Vector> lists;
    lists.push_back({1, 3, 5});
    lists.push_back({2, 4});
    lists.push_back({});
    lists.push_back({8, 9, 10, 11, 12, 13});
    lists.push_back({6});
    lists.push_back({7});
    lists.push_back({});

    const auto result = nx::utils::algorithm::merge_sorted_lists<Vector>(std::move(lists));
    ASSERT_EQ(result, Vector({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}));
}

TEST(MergeSortedLists, DescendingOrder)
{
    std::vector<Vector> lists;
    lists.push_back({1, 3, 5});
    lists.push_back({2, 4});
    lists.push_back({});
    lists.push_back({8, 9, 10, 11, 12, 13});
    lists.push_back({6});
    lists.push_back({7});
    lists.push_back({});

    const auto result = nx::utils::algorithm::merge_sorted_lists<Vector>(std::move(lists),
        [](int value) { return -value; }, Qt::DescendingOrder);

    ASSERT_EQ(result, Vector({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}));
}

TEST(MergeSortedLists, Limit)
{
    std::vector<Vector> lists;
    lists.push_back({1, 3, 5});
    lists.push_back({2, 4});
    lists.push_back({});
    lists.push_back({8, 9, 10, 11, 12, 13});
    lists.push_back({6});
    lists.push_back({7});
    lists.push_back({});

    const auto result = nx::utils::algorithm::merge_sorted_lists<Vector>(std::move(lists),
        Qt::AscendingOrder, 5);

    ASSERT_EQ(result, Vector({1, 2, 3, 4, 5}));
}

TEST(MergeSortedLists, NoLists)
{
    const auto result = nx::utils::algorithm::merge_sorted_lists<Vector>(std::vector<Vector>());
    ASSERT_TRUE(result.empty());
}

TEST(MergeSortedLists, EmptyLists)
{
    std::vector<Vector> lists({Vector(), Vector(), Vector()});
    const auto result = nx::utils::algorithm::merge_sorted_lists<Vector>(lists);
    ASSERT_TRUE(result.empty());
}

TEST(MergeSortedLists, OneList)
{
    std::vector<Vector> lists;
    lists.push_back({10, 20, 30});
    const auto result = nx::utils::algorithm::merge_sorted_lists<Vector>(std::move(lists));
    ASSERT_EQ(result, Vector({10, 20, 30}));
}

TEST(MergeSortedLists, OneEmptyList)
{
    std::vector<Vector> lists({Vector()});
    const auto result = nx::utils::algorithm::merge_sorted_lists<Vector>(lists);
    ASSERT_TRUE(result.empty());
}

TEST(MergeSortedLists, OneNonEmptyList)
{
    std::vector<Vector> lists;
    lists.push_back({});
    lists.push_back({});
    lists.push_back({10, 20, 30});
    const auto result = nx::utils::algorithm::merge_sorted_lists<Vector>(std::move(lists));
    ASSERT_EQ(result, Vector({10, 20, 30}));
}

} // namespace test
} // namespace utils
} // namespace nx
