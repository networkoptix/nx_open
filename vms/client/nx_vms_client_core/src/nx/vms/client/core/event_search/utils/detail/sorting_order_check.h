#pragma once

#include <QtCore/QtGlobal>

namespace nx::vms::client::core::detail {

template<typename Facade, typename Iterator>
bool isSortedCorrectly(const Iterator begin,
    const Iterator end,
    Qt::SortOrder order)
{
    using PredicateType = std::function<bool (const typename Facade::Type& lhs,
        const typename Facade::Type& rhs)>;

    const auto predicate = (order == Qt::AscendingOrder)
        ? PredicateType([](const auto& lhs, const auto& rhs)
            { return Facade::startTime(lhs) < Facade::startTime(rhs); })
        : PredicateType([](const auto& lhs, const auto& rhs)
            { return Facade::startTime(lhs) > Facade::startTime(rhs); });

    return std::is_sorted(begin, end, predicate);
}

template<typename Facade, typename Container>
bool isSortedCorrectly(const Container& container, Qt::SortOrder order)
{
    return isSortedCorrectly<Facade>(container.begin(), container.end(), order);
}

} // namespace nx::vms::client::core::detail
