// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits>

#include <nx/reflect/enum_instrument.h>

template<typename T>
class QFlags;

namespace QnSerialization {

template<typename T>
struct IsFlags: std::false_type {};

template<typename T>
struct IsFlags<QFlags<T>>: std::true_type {};

template<typename T>
struct IsEnumOrFlags:
    std::integral_constant<bool, IsFlags<T>::value || std::is_enum_v<T>>
{};

template<typename T>
struct IsInstrumentedEnumOrFlags
{
    static constexpr bool value = nx::reflect::IsInstrumentedEnumV<T>;
};

template<typename T>
struct IsInstrumentedEnumOrFlags<QFlags<T>>
{
    static constexpr bool value = nx::reflect::IsInstrumentedEnumV<T>;
};

} // namespace QnSerialization
