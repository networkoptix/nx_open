#include <gtest/gtest.h>

#include <vector>
#include <QtCore/QScopedValueRollback>

#include <nx/utils/algorithm/merge_sorted_lists.h>

namespace nx {
namespace utils {
namespace test {

namespace {

class MergeSortedLists: public ::testing::Test
{
protected:
    class Vector: public std::vector<int>
    {
        using base_type = std::vector<int>;

    public:
        Vector(const MergeSortedLists* test = nullptr): base_type(), test(test) {}
        Vector(Vector&& other): base_type(other), test(other.test) {}
        Vector(const Vector& other): base_type(other), test(other.test)
        {
            if (!test || test->m_detectCopy)
                check_deep_copy();
        }

        Vector& operator=(const Vector& other)
        {
            base_type::operator=(other);
            test = other.test;
            if (!test || test->m_detectCopy)
                check_deep_copy();
            return *this;
        }

    private:
        void check_deep_copy() const
        {
            const bool deep_copied = !empty();
            ASSERT_FALSE(deep_copied);
        }

        const MergeSortedLists* test = nullptr;
    };

    Vector createVector(const std::initializer_list<int>& init) const
    {
        auto result = Vector(this);
        result.insert(result.end(), init.begin(), init.end());
        return result;
    }

    std::vector<Vector> testLists()
    {
        QScopedValueRollback<bool> rollback(m_detectCopy, false);
        return std::vector<Vector>{
            createVector({1, 3, 5}),
            createVector({2, 4}),
            createVector({}),
            createVector({8, 9, 10, 11, 12, 13}),
            createVector({6}),
            createVector({7}),
            createVector({})};
    };

    Vector testResult()
    {
        QScopedValueRollback<bool> rollback(m_detectCopy, false);
        return createVector({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13});
    }

    Vector singleTestVector()
    {
        QScopedValueRollback<bool> rollback(m_detectCopy, false);
        return createVector({10, 20, 30});
    }

protected:
    bool m_detectCopy = true;
};

} // namespace

TEST_F(MergeSortedLists, AscendingOrder)
{
    const auto result = nx::utils::algorithm::merge_sorted_lists(testLists());
    ASSERT_EQ(result, testResult());
}

TEST_F(MergeSortedLists, DescendingOrder)
{
    const auto result = nx::utils::algorithm::merge_sorted_lists(testLists(),
        [](int value) { return -value; }, Qt::DescendingOrder);

    ASSERT_EQ(result, testResult());
}

TEST_F(MergeSortedLists, LimitResults)
{
    const auto limit = 5;
    const auto result = nx::utils::algorithm::merge_sorted_lists(testLists(),
        Qt::AscendingOrder, limit);

    auto expected = testResult();
    expected.resize(limit);
    ASSERT_EQ(result, expected);
}

TEST_F(MergeSortedLists, NoLists)
{
    const auto result = nx::utils::algorithm::merge_sorted_lists(std::vector<Vector>());
    ASSERT_TRUE(result.empty());
}

TEST_F(MergeSortedLists, EmptyLists)
{
    std::vector<Vector> lists({createVector({}), createVector({}), createVector({})});
    const auto result = nx::utils::algorithm::merge_sorted_lists(std::move(lists));
    ASSERT_TRUE(result.empty());
}

TEST_F(MergeSortedLists, OneList)
{
    m_detectCopy = false;
    std::vector<Vector> lists({singleTestVector()});
    m_detectCopy = true;
    const auto result = nx::utils::algorithm::merge_sorted_lists(std::move(lists));
    ASSERT_EQ(result, singleTestVector());
}

TEST_F(MergeSortedLists, OneEmptyList)
{
    std::vector<Vector> lists({createVector({})});
    const auto result = nx::utils::algorithm::merge_sorted_lists(lists);
    ASSERT_TRUE(result.empty());
}

TEST_F(MergeSortedLists, OneNonEmptyList)
{
    m_detectCopy = false;
    std::vector<Vector> lists({createVector({}), createVector({}), singleTestVector()});
    m_detectCopy = true;
    const auto result = nx::utils::algorithm::merge_sorted_lists(std::move(lists));
    ASSERT_EQ(result, singleTestVector());
}

} // namespace test
} // namespace utils
} // namespace nx
