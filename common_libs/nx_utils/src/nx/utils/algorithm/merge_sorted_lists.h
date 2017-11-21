#pragma once

#include <limits>
#include <functional>
#include <type_traits>
#include <utility>

#include <QtCore/qnamespace.h>

namespace nx {
namespace utils {
namespace algorithm {

// Merge sorted lists using priority queue concept implemented via STL heap.
// -------------------------------------------------------------------------

// ListOfSortedLists type should be a random access iteratable container of sorted lists.
//   Sorted lists should be of sequentially iteratable container type
//   with value_type type and push_back method defined.

// SortFieldGetter should be a functor taking sorted list value_type and returning a value
//   of type that can be compared with operator <

// sortOrder must be consistent with actual sorting of each sorted list.

// totalLimit is the maximal allowed number of items in the resulting list; ignored if <= 0.

template<class ListOfSortedLists, class SortFieldGetter>
auto merge_sorted_lists(
    ListOfSortedLists sortedLists,
    SortFieldGetter sortFieldGetter,
    Qt::SortOrder sortOrder = Qt::AscendingOrder,
    int totalLimit = std::numeric_limits<int>::max())
{
    using SortedList = std::remove_reference<decltype(*sortedLists.begin())>::type;

    switch (sortedLists.size())
    {
        case 0: return SortedList();
        case 1: return std::move(*sortedLists.begin());
        default: break;
    }

    using Iterator = typename SortedList::iterator;
    using IteratorRange = std::pair<Iterator, Iterator>;

    std::vector<IteratorRange> queueData;
    queueData.reserve(sortedLists.size());

    SortedList* lastNonEmptyList = nullptr;
    for (auto& list: sortedLists)
    {
        if (list.empty())
            continue;

        lastNonEmptyList = &list;
        queueData.push_back(std::make_pair(list.begin(), list.end()));
    }

    switch (queueData.size())
    {
        case 0: return SortedList();
        case 1: return std::move(*lastNonEmptyList);
        default: break;
    }

    SortedList result;
    static constexpr int kMaximumReserve = 10000; // Something sensible.

    if (totalLimit <= 0)
        totalLimit = std::numeric_limits<int>::max();

    result.reserve(std::min(totalLimit, kMaximumReserve));

    std::function<bool(const IteratorRange&, const IteratorRange&)> lessPriority;

    if (sortOrder == Qt::DescendingOrder)
    {
        lessPriority =
            [sortFieldGetter](const IteratorRange& left, const IteratorRange& right) -> bool
            {
                return sortFieldGetter(*left.first) < sortFieldGetter(*right.first);
            };
    }
    else
    {
        lessPriority =
            [sortFieldGetter](const IteratorRange& left, const IteratorRange& right) -> bool
            {
                return sortFieldGetter(*left.first) > sortFieldGetter(*right.first);
            };
    }

    std::make_heap(queueData.begin(), queueData.end(), lessPriority);

    while (!queueData.empty() && result.size() < totalLimit)
    {
        std::pop_heap(queueData.begin(), queueData.end(), lessPriority);
        auto& range = queueData.back();

        result.push_back(*range.first);
        ++range.first;

        if (range.first == range.second)
            queueData.pop_back();
        else
            std::push_heap(queueData.begin(), queueData.end(), lessPriority);
    }

    return std::move(result);
};

template<class ListOfSortedLists>
auto merge_sorted_lists(
    ListOfSortedLists sortedLists,
    Qt::SortOrder sortOrder = Qt::AscendingOrder,
    int totalLimit = std::numeric_limits<int>::max())
{
    using SortedList = std::remove_reference<decltype(*sortedLists.begin())>::type;

    return merge_sorted_lists(std::move(sortedLists),
        [](typename SortedList::value_type& value) { return value; },
        sortOrder, totalLimit);
}

} // namespace algorithm
} // namespace utils
} // namespace nx
