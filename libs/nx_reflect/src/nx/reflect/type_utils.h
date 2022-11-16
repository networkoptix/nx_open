// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <set>
#include <type_traits>
#include <unordered_set>

namespace nx::reflect {

/**
 * A simplified replacement for Container concept (until we have c++20 concepts).
 */
template<
    typename,
    typename = std::void_t<>,
    typename = std::void_t<>,
    typename = std::void_t<>
>
struct IsContainer: std::false_type {};

template<typename T>
struct IsContainer<
    T,
    std::void_t<decltype(std::declval<T>().begin())>,
    std::void_t<decltype(std::declval<T>().end())>,
    std::void_t<typename T::value_type>
>: std::true_type {};

template<typename... Args>
inline constexpr bool IsContainerV = IsContainer<Args...>::value;

//-------------------------------------------------------------------------------------------------

namespace detail {

template<typename T, typename = std::void_t<>>
struct HasHasher: std::false_type {};

template<typename T>
struct HasHasher<T, std::void_t<typename T::hasher>>:
    std::true_type
{
};

template<typename... Args>
inline constexpr bool HasHasherV = HasHasher<Args...>::value;

} // namespace detail


/**
 * A simplified replacement for AssociativeContainer concept (until we have c++20 concepts).
 */
template<
    typename T,
    typename = std::void_t<>,
    typename = std::void_t<>,
    typename = std::void_t<>,
    typename = std::void_t<>
>
struct IsAssociativeContainer: std::false_type {};

template<typename T>
struct IsAssociativeContainer<
    T,
    std::void_t<std::enable_if_t<IsContainerV<T>>>,
    std::void_t<typename T::key_type>,
    std::void_t<typename T::key_compare>,
    // NOTE: It is wierd, but msvc v141 defines key_compare and value_compare in
    // unordered containers. So, taking this into account.
    std::void_t<std::enable_if_t<!detail::HasHasherV<T>>>
>: std::true_type {};

template<typename... Args>
inline constexpr bool IsAssociativeContainerV = IsAssociativeContainer<Args...>::value;

//-------------------------------------------------------------------------------------------------

namespace detail {

template<typename T, typename = std::void_t<>>
struct HasMappedType: std::false_type {};

template<typename T>
struct HasMappedType<T, std::void_t<typename T::mapped_type>>:
    std::true_type {};

template<typename... Args>
inline constexpr bool HasMappedTypeV = HasMappedType<Args...>::value;

} // namespace detail

template<
    typename T,
    typename = std::void_t<>,
    typename = std::void_t<>
>
struct IsSetContainer: std::false_type {};

/**
 * Tests for std::set and std::multiset.
 * NOTE: there is no a similar concept in the STL.
 */
template<typename T>
struct IsSetContainer<
    T,
    std::void_t<std::enable_if_t<IsAssociativeContainerV<T>>>,
    std::void_t<std::enable_if_t<!detail::HasMappedTypeV<T>>>
>: std::true_type {};

template<typename... Args>
inline constexpr bool IsSetContainerV = IsSetContainer<Args...>::value;

//-------------------------------------------------------------------------------------------------

/**
 * A simplified replacement for UnorderedAssociativeContainer concept (until we have c++20 concepts).
 */
template<
    typename T,
    typename = std::void_t<>,
    typename = std::void_t<>,
    typename = std::void_t<>
>
struct IsUnorderedAssociativeContainer: std::false_type {};

template<typename T>
struct IsUnorderedAssociativeContainer<
    T,
    std::void_t<std::enable_if_t<IsContainerV<T>>>,
    std::void_t<typename T::key_type>,
    std::void_t<typename T::hasher>
>: std::true_type
{
};

template<typename... Args>
inline constexpr bool IsUnorderedAssociativeContainerV =
    IsUnorderedAssociativeContainer<Args...>::value;

//-------------------------------------------------------------------------------------------------

template<
    typename T,
    typename = std::void_t<>,
    typename = std::void_t<>
>
struct IsUnorderedSetContainer: std::false_type {};

template<typename T>
struct IsUnorderedSetContainer<
    T,
    std::void_t<std::enable_if_t<IsUnorderedAssociativeContainerV<T>>>,
    std::void_t<std::enable_if_t<!detail::HasMappedTypeV<T>>>
>: std::true_type
{
};

template<typename... Args>
inline constexpr bool IsUnorderedSetContainerV = IsUnorderedSetContainer<Args...>::value;

//-------------------------------------------------------------------------------------------------

/**
 * A simplified replacement for SequenceContainer concept (until we have c++20 concepts).
 */
template<
    typename T,
    typename = std::void_t<>,
    typename = std::void_t<>
>
struct IsSequenceContainer: std::false_type {};

template<typename T>
struct IsSequenceContainer<
    T,
    std::enable_if_t<IsContainerV<T>>,
    std::void_t<decltype(std::declval<T>().push_back(std::declval<typename T::value_type>()))>
>: std::true_type {};

template<typename... Args>
inline constexpr bool IsSequenceContainerV = IsSequenceContainer<Args...>::value;

//-------------------------------------------------------------------------------------------------

template<typename... Args>
struct IsStdChronoDuration: std::false_type {};

template<typename... Args>
struct IsStdChronoDuration<std::chrono::duration<Args...>>: std::true_type {};

template<typename... Args>
constexpr bool IsStdChronoDurationV = IsStdChronoDuration<Args...>::value;

//-------------------------------------------------------------------------------------------------

template<typename... Args>
struct IsStdChronoTimePoint: std::false_type {};

template<typename... Args>
struct IsStdChronoTimePoint<std::chrono::time_point<Args...>>: std::true_type {};

template<typename... Args>
constexpr bool IsStdChronoTimePointV = IsStdChronoTimePoint<Args...>::value;

} // namespace nx::reflect
