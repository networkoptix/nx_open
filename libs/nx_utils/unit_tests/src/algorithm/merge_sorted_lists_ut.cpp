// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <vector>

#include <QtCore/QScopedValueRollback>

#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/to_string.h>
#include <nx/utils/algorithm/merge_sorted_lists.h>

namespace nx {
namespace utils {
namespace test {

namespace {

NX_REFLECTION_ENUM(SortOrder,
    AscendingOrder,
    DescendingOrder
);

NX_REFLECTION_ENUM(Duplicates,
    KeepDuplicates,
    RemoveDuplicates
);

void PrintTo(SortOrder value, std::ostream* os) { *os << nx::reflect::toString(value); }
void PrintTo(Duplicates value, std::ostream* os) { *os << nx::reflect::toString(value); }

class MergeSortedLists: public ::testing::TestWithParam<std::tuple<SortOrder, Duplicates, int>>
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

    // Test call with parameters.
    auto merge_sorted_lists(auto sourceLists) const
    {
        return nx::utils::algorithm::merge_sorted_lists(
            std::move(sourceLists),
            sortOrderParam(),
            limitParam(),
            removeDuplicatesParam());
    }

    const std::vector<int>& sourceSingleList() const
    {
        static const std::vector<int> value{1, 2, 2, 3, 4, 4, 5, 5, 5};
        return value;
    }

    const std::vector<std::vector<int>>& sourceMultipleLists() const
    {
        static const std::vector<std::vector<int>> value{
            {1, 3, 5},
            {2, 3, 3, 4},
            {},
            {8, 9, 10, 11, 11, 12, 13},
            {6},
            {1, 7, 7, 13},
            {}};

        return value;
    };

    const std::vector<int>& referenceMergedMultipleLists() const
    {
        static const std::vector<int> value =
            [this]()
            {
                std::vector<int> result;
                for (const auto& list: sourceMultipleLists())
                    result.insert(result.end(), list.begin(), list.end());
                std::sort(result.begin(), result.end());
                return result;
            }();

        return value;
    }

    std::vector<Vector> theOnlyListSource() const
    {
        QScopedValueRollback<bool> rollback(m_detectCopy, false);
        return std::vector<Vector>{prepareSource(sourceSingleList())};
    }

    std::vector<int> theOnlyListMergeResult() const
    {
        return processReferenceResult(sourceSingleList());
    }

    std::vector<Vector> oneNonEmptyListSource() const
    {
        QScopedValueRollback<bool> rollback(m_detectCopy, false);
        return std::vector<Vector>{{}, prepareSource(sourceSingleList()), {}};
    }

    std::vector<int> oneNonEmptyListMergeResult() const
    {
        return processReferenceResult(sourceSingleList());
    }

    std::vector<Vector> multipleListsSource() const
    {
        QScopedValueRollback<bool> rollback(m_detectCopy, false);

        std::vector<Vector> result;
        result.reserve(sourceMultipleLists().size());

        for (const auto& list: sourceMultipleLists())
            result.push_back(prepareSource(createVector(list)));

        return result;
    }

    std::vector<int> multipleListsMergeResult() const
    {
        return processReferenceResult(referenceMergedMultipleLists());
    }

private:
    Qt::SortOrder sortOrderParam() const
    {
        return std::get<0>(GetParam()) == AscendingOrder
            ? Qt::AscendingOrder
            : Qt::DescendingOrder;
    }

    bool removeDuplicatesParam() const
    {
        return std::get<1>(GetParam()) == RemoveDuplicates;
    }

    int limitParam() const
    {
        return std::get<2>(GetParam());
    }

    Vector createVector(const std::vector<int>& init) const
    {
        auto result = Vector(this);
        result.insert(result.end(), init.begin(), init.end());
        return result;
    }

    Vector prepareSource(std::vector<int> source) const
    {
        // The source must be sorted in ascending order, otherwise the test is malformed.
        EXPECT_TRUE(std::is_sorted(source.cbegin(), source.cend()));

        if (sortOrderParam() == Qt::DescendingOrder)
            std::reverse(source.begin(), source.end());

        return createVector(source);
    }

    std::vector<int> processReferenceResult(std::vector<int> result) const
    {
        // The reference result must be sorted in ascending order, otherwise the test is malformed.
        EXPECT_TRUE(std::is_sorted(result.cbegin(), result.cend()));

        if (removeDuplicatesParam())
            result.erase(std::unique(result.begin(), result.end()), result.end());

        if (sortOrderParam() == Qt::DescendingOrder)
            std::reverse(result.begin(), result.end());

        if ((int) result.size() > limitParam())
            result.resize(limitParam());

        return result;
    }

protected:
    mutable bool m_detectCopy = true;
};

} // namespace

TEST_P(MergeSortedLists, NoLists)
{
    const auto result = merge_sorted_lists(std::vector<Vector>{});
    ASSERT_TRUE(result.empty());
}

TEST_P(MergeSortedLists, OneEmptyList)
{
    const auto result = merge_sorted_lists(std::vector<Vector>{{}});
    ASSERT_TRUE(result.empty());
}

TEST_P(MergeSortedLists, EmptyLists)
{
    const auto result = merge_sorted_lists(std::vector<Vector>{{}, {}, {}});
    ASSERT_TRUE(result.empty());
}

TEST_P(MergeSortedLists, OneList)
{
    const auto result = merge_sorted_lists(theOnlyListSource());
    ASSERT_EQ(result, theOnlyListMergeResult());
}

TEST_P(MergeSortedLists, OneNonEmptyList)
{
    const auto result = merge_sorted_lists(oneNonEmptyListSource());
    ASSERT_EQ(result, oneNonEmptyListMergeResult());
}

TEST_P(MergeSortedLists, MultipleLists)
{
    const auto result = merge_sorted_lists(multipleListsSource());
    ASSERT_EQ(result, multipleListsMergeResult());
}

TEST_P(MergeSortedLists, NonDestructive)
{
    // Ensure that when the source list of lists is passed as a non-const l-value reference,
    // the merge operation doesn't destroy or modify it.

    m_detectCopy = false;

    auto source = multipleListsSource();
    const auto sourceCopy = source;
    merge_sorted_lists(source); //< Passed as l-value reference.
    ASSERT_EQ(source, sourceCopy); //< `source` is not changed.
}

INSTANTIATE_TEST_SUITE_P(MergeSortedLists,MergeSortedLists,
    ::testing::Combine(
        ::testing::Values(AscendingOrder, DescendingOrder),
        ::testing::Values(KeepDuplicates, RemoveDuplicates),
        ::testing::Values(std::numeric_limits<int>::max(), 3, 10)));

} // namespace test
} // namespace utils
} // namespace nx
