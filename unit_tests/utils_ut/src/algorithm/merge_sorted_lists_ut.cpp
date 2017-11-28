#include <gtest/gtest.h>

#include <vector>
#include <QtCore/QScopedValueRollback>

#include <nx/utils/algorithm/merge_sorted_lists.h>

namespace nx {
namespace utils {
namespace test {

namespace {

class Vector: public std::vector<int>
{
    using base_type = std::vector<int>;

public:
    Vector(): base_type() {}
    Vector(Vector&& other): base_type(other) {}
    Vector(const std::initializer_list<int>& init): base_type(init) {}
    Vector(const Vector& other): base_type(other)
    {
        if (detectCopy)
            check_deep_copy();
    }

    static thread_local bool detectCopy;

private:
    void check_deep_copy() const
    {
        const bool deep_copied = !empty();
        ASSERT_FALSE(deep_copied);
    }
};

thread_local bool Vector::detectCopy = true;

std::vector<Vector> testLists()
{
    QScopedValueRollback<bool> rollback(Vector::detectCopy, false);
    return std::vector<Vector>{
        {1, 3, 5},
        {2, 4},
        {},
        {8, 9, 10, 11, 12, 13},
        {6},
        {7},
        {}};
}

Vector testResult()
{
    return Vector({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13});
}

Vector singleTestVector()
{
    return Vector({10, 20, 30});
}

} // namespace

TEST(MergeSortedLists, AscendingOrder)
{
    const auto result = nx::utils::algorithm::merge_sorted_lists(testLists());
    ASSERT_EQ(result, testResult());
}

TEST(MergeSortedLists, DescendingOrder)
{
    const auto result = nx::utils::algorithm::merge_sorted_lists(testLists(),
        [](int value) { return -value; }, Qt::DescendingOrder);

    ASSERT_EQ(result, testResult());
}

TEST(MergeSortedLists, Limit)
{
    const auto limit = 5;
    const auto result = nx::utils::algorithm::merge_sorted_lists(testLists(),
        Qt::AscendingOrder, limit);

    auto expected = testResult();
    expected.resize(limit);
    ASSERT_EQ(result, expected);
}

TEST(MergeSortedLists, NoLists)
{
    const auto result = nx::utils::algorithm::merge_sorted_lists(std::vector<Vector>());
    ASSERT_TRUE(result.empty());
}

TEST(MergeSortedLists, EmptyLists)
{
    std::vector<Vector> lists({Vector(), Vector(), Vector()});
    const auto result = nx::utils::algorithm::merge_sorted_lists(std::move(lists));
    ASSERT_TRUE(result.empty());
}

TEST(MergeSortedLists, OneList)
{
    Vector::detectCopy = false;
    std::vector<Vector> lists({singleTestVector()});
    Vector::detectCopy = true;
    const auto result = nx::utils::algorithm::merge_sorted_lists(std::move(lists));
    ASSERT_EQ(result, singleTestVector());
}

TEST(MergeSortedLists, OneEmptyList)
{
    std::vector<Vector> lists({Vector()});
    const auto result = nx::utils::algorithm::merge_sorted_lists(lists);
    ASSERT_TRUE(result.empty());
}

TEST(MergeSortedLists, OneNonEmptyList)
{
    Vector::detectCopy = false;
    std::vector<Vector> lists({{}, {}, singleTestVector()});
    Vector::detectCopy = true;
    const auto result = nx::utils::algorithm::merge_sorted_lists(std::move(lists));
    ASSERT_EQ(result, singleTestVector());
}

} // namespace test
} // namespace utils
} // namespace nx
