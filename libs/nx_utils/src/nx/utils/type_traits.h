// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <optional>
#include <type_traits>
#include <vector>

namespace nx::utils {

template<typename T, typename = void>
struct isIterable: std::false_type {};

template<typename T>
struct isIterable<
    T,
    std::void_t<
        decltype(std::begin(std::declval<T>())),
        decltype(std::end(std::declval<T>()))
    >
>: std::true_type {};

template<typename... U> inline constexpr bool IsIterableV = isIterable<U...>::value;

//-------------------------------------------------------------------------------------------------

template<typename T, typename = void>
struct IsIterator: std::false_type {};

template<typename T>
struct IsIterator<T, std::void_t<typename std::iterator_traits<T>::value_type>>: std::true_type {};

template<typename... U> inline constexpr bool IsIteratorV = IsIterator<U...>::value;

template<typename>
struct IsOptional: std::false_type {};

template<typename T>
struct IsOptional<std::optional<T>>: std::true_type {};

template<typename>
struct IsVector: std::false_type {};

template<typename T, typename A>
struct IsVector<std::vector<T, A>>: std::true_type {};

template<typename, typename = std::void_t<>>
struct HasGetId: std::false_type {};
template<typename T>
struct HasGetId<T, std::void_t<decltype(&T::getId)>>: std::true_type {};

template<typename, typename = std::void_t<>>
struct HasSetId: std::false_type {};
template<typename T>
struct HasSetId<T, std::void_t<decltype(&T::setId)>>: std::true_type {};

template <typename T>
struct HasGetIdAndSetId
{
    static constexpr bool value = HasGetId<T>::value && HasSetId<T>::value;
};

template <typename T> using IsCreateModel = HasGetIdAndSetId<T>;
template <typename T> using IsUpdateModel = HasGetId<T>;
template <typename T> using IsFlexibleIdModel = HasGetIdAndSetId<T>;

template <typename T> constexpr bool isCreateModelV = IsCreateModel<T>::value;
template <typename T> constexpr bool isUpdateModelV = IsUpdateModel<T>::value;
template <typename T> constexpr bool isFlexibleIdModelV = IsFlexibleIdModel<T>::value;

} // namespace nx::utils

namespace QnTypeTraits {

    struct na {};

    template<class T>
    struct identity {
        typedef T type;
    };

    template<class T>
    struct remove_cvr:
        std::remove_cv<
            typename std::remove_reference<T>::type
        > {};

    struct yes_type { char dummy; };
    struct no_type { char dummy[64]; };

} // namespace QnTypeTraits
