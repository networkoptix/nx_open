// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>
#include <map>
#include <vector>

namespace nx {
namespace utils {

template<typename ForwardIt, typename T>
ForwardIt binary_find(ForwardIt first, ForwardIt last, const T& value)
{
    auto lbit = std::lower_bound(first, last, value);
    if (lbit != last && !(value < *lbit))
        return lbit;

    return last;
}

template<typename ForwardIt, typename T, typename Compare>
ForwardIt binary_find(ForwardIt first, ForwardIt last, const T& value, Compare comp)
{
    auto lbit = std::lower_bound(first, last, value, comp);
    if (lbit != last && !comp(value, *lbit))
        return lbit;

    return last;
}

/**
 * Elements from [first, last) range matching p are moved to outFirst.
 * And such element cell is moved to the end of [first, last) range.
 * @return Iterator to the end of new range without elements moved.
 */
template<typename InputIt, class OutputIt, typename UnaryPredicate>
InputIt move_if(InputIt first, InputIt last, OutputIt outFirst, UnaryPredicate p)
{
    using ValueType = decltype(*first);

    InputIt newRangeEndIt = std::stable_partition(
        first, last,
        [&p](const ValueType& elem)
        {
            return !p(elem);
        });
    for (auto it = newRangeEndIt; it != last; ++it)
        *outFirst++ = std::move(*it);

    return newRangeEndIt;
}

template<typename InputIt, typename T>
bool contains(InputIt first, InputIt last, const T& value)
{
    using RangeElement = decltype(*first);
    return std::any_of(
        first, last,
        [&value](const RangeElement& elementValue)
        {
            return elementValue == value;
        });
}

template<typename Container, typename T>
bool contains(const Container& container, const T& value)
{
    return contains(container.begin(), container.end(), value);
}

template<typename Key, typename T, typename... Args>
bool contains(const std::map<Key, Args...>& container, const T& key)
{
    return container.find(key) != container.end();
}

template<typename InputIt, typename UnaryPredicate>
bool contains_if(InputIt first, InputIt last, UnaryPredicate p)
{
    return std::any_of(first, last, p);
}

template<typename Container, typename UnaryPredicate>
bool contains_if(const Container& container, UnaryPredicate p)
{
    return contains_if(container.begin(), container.end(), p);
}

//-------------------------------------------------------------------------------------------------

template<typename Container, typename UnaryPredicate>
size_t erase_if(Container& container, UnaryPredicate p)
{
    const auto removed = std::remove_if(container.begin(), container.end(), p);
    const auto count = std::distance(removed, container.end());
    container.erase(removed, container.end());
    return count;
}

template<typename Container, typename UnaryPredicate>
Container copy_if(Container values, UnaryPredicate filter)
{
    erase_if(values, [&filter](const auto& v) { return !filter(v); });
    return values;
}

template<typename Key, typename Value, typename UnaryPredicate>
std::map<Key, Value> copy_if(std::map<Key, Value> values, UnaryPredicate filter)
{
    std::erase_if(values,
        [&filter](const auto& item) { auto const& [k, v] = item; return !filter(k, v); });
    return values;
}

template<typename Container, typename UnaryPredicate>
const typename Container::value_type* find_if(const Container& container, UnaryPredicate p)
{
    const auto it = std::find_if(container.begin(), container.end(), p);
    return it == container.end() ? (typename Container::value_type*) nullptr : &(*it);
}

template<typename Container, typename UnaryPredicate>
typename Container::value_type* find_if(Container& container, UnaryPredicate p)
{
    const auto it = std::find_if(container.begin(), container.end(), p);
    return it == container.end() ? (typename Container::value_type*) nullptr : &(*it);
}

template<typename Key, typename T>
auto flat_map(std::map<Key, T> values, Key /*delimiter*/)
{
    return values;
}

template<typename Key, typename T>
auto flat_map(std::map<Key, std::vector<T>> values, Key delimiter = ".")
{
    std::map<Key, T> result;
    for (auto& [key, vector]: values)
    {
        for (std::size_t i = 0; i < vector.size(); ++i)
            result[key + delimiter + QString::number(i)] = std::move(vector[i]);
    }
    return flat_map(std::move(result), delimiter);
}

template<typename Key, typename T>
auto flat_map(std::map<Key, std::map<Key, T>> values, Key delimiter = ".")
{
    std::map<Key, T> result;
    for (auto& [key, subValues]: values)
    {
        for (auto& [subKey, value]: subValues)
            result[key + delimiter + subKey] = std::move(value);
    }
    return flat_map(std::move(result), delimiter);
}

namespace detail {

template<class Functor>
class y_combinator_result
{
    Functor functor;

public:
    y_combinator_result() = delete;
    y_combinator_result(const y_combinator_result&) = delete;

    template<class T>
    explicit y_combinator_result(T&& impl): functor(std::forward<T>(impl)) {}

    template<class... Args>
    decltype(auto) operator()(Args&&... args) const
    {
        return functor(std::cref(*this), std::forward<Args>(args)...);
    }
};

} // namespace detail

/**
 * Standard Y-combinator implementation to create recursive lambdas without an explicit declaration
 * of the std::function arguments and the return type.
 * Usage example:
 * auto recursive_lambda = y_combinator([](auto recursive_lambda) { recursive_lambda(); });
 * Implementation is taken from the P0200R0.
 */
template<class Functor>
decltype(auto) y_combinator(Functor&& functor)
{
    return detail::y_combinator_result<std::decay_t<Functor>>(std::forward<Functor>(functor));
}

/**
 * Sorts and removes duplicate elements of the `vector` in place using a comparison function.
 * @param vec vector to sort
 * @param comp comparison function object (i.e. an object that satisfies the requirements of
 *     Compare) which returns `true` if the first argument is less than (i.e. is ordered before)
 *     the second.<br>
 *     The signature of the comparison function should be equivalent to the following:<br>
 *     `bool cmp(const Type1& a, const Type2& b);`
 */
template <typename T, typename Allocator, typename Compare>
void unique_sort(std::vector<T, Allocator> &vec, Compare comp)
{
    std::sort(vec.begin(), vec.end(), comp);
    vec.erase(
        std::unique(vec.begin(), vec.end(),
            [comp](const auto& lhs, const auto& rhs) -> bool
            {
                return !comp(lhs, rhs) && !comp(rhs, lhs);
            }), vec.end());
}

/**
 * Sorts and removes duplicate elements of the `vector` in place.
 * @param vec vector to sort
 */
template <typename T, typename Allocator>
void unique_sort(std::vector<T, Allocator> &vec)
{
    return unique_sort(vec, std::less<T>{});
}

/**
 * Returns a sorted vector of items without duplicates.
 * @param items container to sort
 * @param comp comparison function object (i.e. an object that satisfies the requirements of
 *     Compare) which returns `true` if the first argument is less than (i.e. is ordered before)
 *     the second.<br>
 *     The signature of the comparison function should be equivalent to the following:<br>
 *     `bool cmp(const Type1& a, const Type2& b);`
 * @return sorted vector of unique items.
 */
template <typename Container, typename Compare>
[[nodiscard]] auto unique_sorted(Container items, Compare comp)
{
    using value_type = typename std::decay_t<Container>::value_type;
    std::vector<value_type> result(
        std::move_iterator(items.begin()), std::move_iterator(items.end()));
    unique_sort(result, comp);
    return result;
}

/**
 * Returns a sorted vector of items without duplicates.
 * @param items container to sort
 * @return sorted vector of unique items.
 */
template <typename Container>
[[nodiscard]] auto unique_sorted(Container items)
{
    using value_type = typename std::decay_t<Container>::value_type;
    std::vector<value_type> result(
        std::move_iterator(items.begin()), std::move_iterator(items.end()));
    unique_sort(result);
    return result;
}

/**
 * Returns a sorted vector of items without duplicates.
 * @param items vector to sort
 * @param comp comparison function object (i.e. an object that satisfies the requirements of
 *     Compare) which returns `true` if the first argument is less than (i.e. is ordered before)
 *     the second.<br>
 *     The signature of the comparison function should be equivalent to the following:<br>
 *     `bool cmp(const Type1& a, const Type2& b);`
 * @return sorted vector of unique items.
 *
 * @example `items = unique_sorted(std::move(items), isLess);`
 */
template <typename Compare, typename T, typename Allocator>
[[nodiscard]] std::vector<T, Allocator> unique_sorted(std::vector<T, Allocator> items, Compare comp)
{
    unique_sort(items, comp);
    return items;
}

/**
 * Returns a sorted vector of items without duplicates.
 * @param items vector to sort
 * @return sorted vector of unique items.
 *
 * @example `items = unique_sorted(std::move(items));`
 */
template <typename T, typename Allocator>
[[nodiscard]] std::vector<T, Allocator> unique_sorted(std::vector<T, Allocator> items)
{
    unique_sort(items);
    return items;
}

} // namespace utils
} // namespace nx
