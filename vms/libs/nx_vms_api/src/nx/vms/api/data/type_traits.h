// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits>

namespace nx {
namespace vms {
namespace api {

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

} // namespace api
} // namespace vms
} // namespace nx
