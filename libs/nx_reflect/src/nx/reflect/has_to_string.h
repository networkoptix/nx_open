// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::reflect::detail {

template<typename T>
inline constexpr bool HasToStdStringV = requires(T value) { value.toStdString(); };

template<typename T>
inline constexpr bool HasToStringV = requires(T value) { value.toString(); };

template<typename T>
inline constexpr bool HasFreeStandToStringV =
    requires(T&& value) { toString(std::forward<T>(value)); };

} // namespace nx::reflect::detail
