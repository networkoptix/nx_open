#include <gtest/gtest.h>

#include <vector>
#include <QtCore/QScopedValueRollback>

#include <nx/utils/algorithm/truncate_sorted_lists.h>

namespace nx::utils::test {

namespace {

class TruncateSortedLists: public ::testing::Test
{
protected:
    using Vector = std::vector<int>;

    std::vector<Vector> testLists()
    {
        return std::vector<Vector>{
            Vector({1, 3, 5}),
            Vector({2, 4}),
            Vector({}),
            Vector({8, 9, 10, 11, 12, 13}),
            Vector({6}),
            Vector({7}),
            Vector({})};
    };

    std::vector<Vector> testResultFor3()
    {
        return std::vector<Vector>{
            Vector({1, 3}),
            Vector({2}),
            Vector({}),
            Vector({}),
            Vector({}),
            Vector({}),
            Vector({})};
    };

    std::vector<Vector> testResultFor10()
    {
        return std::vector<Vector>{
            Vector({1, 3, 5}),
            Vector({2, 4}),
            Vector({}),
            Vector({8, 9, 10}),
            Vector({6}),
            Vector({7}),
            Vector({})};
    };

    Vector singleTestVector()
    {
        return Vector({10, 20, 30, 40, 50});
    }

    Vector singleTestVectorTruncated(int limit)
    {
        auto result = singleTestVector();
        result.resize(std::min(result.size(), size_t(limit)));
        return result;
    }
};

} // namespace

TEST_F(TruncateSortedLists, AscendingOrder)
{
    auto lists = testLists();
    int count = nx::utils::algorithm::truncate_sorted_lists(lists, 3);
    ASSERT_EQ(count, 3);
    ASSERT_EQ(lists, testResultFor3());
}

TEST_F(TruncateSortedLists, DescendingOrder)
{
    auto lists = testLists();
    int count = nx::utils::algorithm::truncate_sorted_lists(lists,
        [](int value) { return -value; }, 10, Qt::DescendingOrder);
    ASSERT_EQ(count, 10);
    ASSERT_EQ(lists, testResultFor10());
}

TEST_F(TruncateSortedLists, NoTruncation)
{
    auto lists = testLists();
    int count = nx::utils::algorithm::truncate_sorted_lists(lists, 3);
    ASSERT_EQ(count, 3);
    ASSERT_EQ(lists, testResultFor3());
}

TEST_F(TruncateSortedLists, ZeroTruncation)
{
    auto lists = testLists();
    const auto sourceListsCount = lists.size();
    int count = nx::utils::algorithm::truncate_sorted_lists(lists, 0);
    ASSERT_EQ(count, 0);
    ASSERT_EQ(lists, std::vector<Vector>(sourceListsCount));
}

TEST_F(TruncateSortedLists, OneList)
{
    std::vector<Vector> lists({singleTestVector()});
    int count = nx::utils::algorithm::truncate_sorted_lists(lists, 3);
    ASSERT_EQ(count, 3);
    ASSERT_EQ(lists, std::vector<Vector>({singleTestVectorTruncated(3)}));
}

TEST_F(TruncateSortedLists, OneListNoTruncation)
{
    const auto source = singleTestVector();
    std::vector<Vector> lists({source});
    int count = nx::utils::algorithm::truncate_sorted_lists(lists, (int) source.size());
    ASSERT_EQ(count, source.size());
    ASSERT_EQ(lists, std::vector<Vector>({source}));
}

TEST_F(TruncateSortedLists, SourceListOfPointers)
{
    auto l1 = Vector({10, 1});
    auto l2 = Vector({20, 2});
    auto l3 = Vector({30, 3});
    const auto lists = {&l1, &l2, &l3};
    int count = nx::utils::algorithm::truncate_sorted_lists(lists, 3, Qt::DescendingOrder);
    ASSERT_EQ(count, 3);
    ASSERT_EQ(l1, Vector({10}));
    ASSERT_EQ(l2, Vector({20}));
    ASSERT_EQ(l3, Vector({30}));
}

} // namespace nx::utils::test
