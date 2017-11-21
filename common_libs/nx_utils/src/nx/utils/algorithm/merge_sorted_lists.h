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

// SortedList type should be a vector/deque type with value_type type, push_back function
//   and random access iterators.

// ListOfLists type should be a sequentially iteratable container of SortedList items.

// SortFieldGetter should be a functor taking SortedList::value_type and returning value that
//   can be compared with operator <

// sortOrder must be consistent with actual sorting of each sorted list.

// totalLimit is the maximal allowed number of items in the resulting list; ignored if <= 0.

template<class SortedList, class ListOfLists, class SortFieldGetter>
SortedList merge_sorted_lists(
    ListOfLists sortedLists,
    SortFieldGetter sortFieldGetter,
    Qt::SortOrder sortOrder = Qt::AscendingOrder,
    int totalLimit = std::numeric_limits<int>::max())
{
    switch (sortedLists.size())
    {
        case 0: return SortedList();
        case 1: return std::move(sortedLists.front());
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

template<class SortedList, class ListOfLists>
SortedList merge_sorted_lists(
    ListOfLists sortedLists,
    Qt::SortOrder sortOrder = Qt::AscendingOrder,
    int totalLimit = std::numeric_limits<int>::max())
{
    return merge_sorted_lists<SortedList>(std::move(sortedLists),
        [](typename SortedList::value_type& value) { return value; },
        sortOrder, totalLimit);
}

} // namespace algorithm
} // namespace utils
} // namespace nx
