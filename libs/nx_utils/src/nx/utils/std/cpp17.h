#pragma once

#include <cstddef>
#include <initializer_list>

#include "cpp14.h"

/**@file
 * Some c++17 features easy to implement using c++14 defined here.
 * Many features that are constexpr in c++17, are not constexpr in c++14, so we need to omit
 * constexpr in many cases.
 */

namespace std {

//-------------------------------------------------------------------------------------------------
// size(X) global function. Works for STL-compatible containers, initializer_list and c-array.
// Returns size of parameter in elements.

template <class C>
auto size(const C& c) -> decltype(c.size())
{
    return c.size();
}

template <class T, std::size_t N>
std::size_t size(const T(&array)[N]) noexcept
{
    return N;
}

//-------------------------------------------------------------------------------------------------
// empty(X) global function. Works for STL-compatible containers, initializer_list and c-array
// Returns true if parameter contains no elements, false otherwise.

template <class C>
auto empty(const C& c) -> decltype(c.empty())
{
    return c.empty();
}

template <class T, std::size_t N>
constexpr bool empty(const T(&array)[N]) noexcept
{
    return false;
}

template <class E>
constexpr bool empty(std::initializer_list<E> il) noexcept
{
    return il.size() == 0;
}

//-------------------------------------------------------------------------------------------------
// data(X) global function. Works for array, vector, (w)string, initializer_list and c-array
// Returns the address of memory containing elements.

template <class C>
constexpr auto data(C& c) -> decltype(c.data())
{
    return c.data();
}

template <class C>
constexpr auto data(const C& c) -> decltype(c.data())
{
    return c.data();
}

template <class T, std::size_t N>
constexpr T* data(T(&array)[N]) noexcept
{
    return array;
}

template <class E>
constexpr const E* data(std::initializer_list<E> il) noexcept
{
    return il.begin();
}

//-------------------------------------------------------------------------------------------------
// clamp standard algorithm.
// If v less than lo, returns lo; otherwise if hi less than v, returns hi; otherwise returns v.

template<class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi)
{
    return clamp(v, lo, hi, std::less<>());
}

template<class T, class Compare>
constexpr const T& clamp(const T& v, const T& lo, const T& hi, Compare comp)
{
    assert(!comp(hi, lo));
    return comp(v, lo) ? lo : comp(hi, v) ? hi : v;
}

//-------------------------------------------------------------------------------------------------

} // namespace std
