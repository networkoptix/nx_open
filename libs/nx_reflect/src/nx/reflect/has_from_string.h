// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx::reflect::detail {

template<typename T>
inline constexpr bool HasFromStdStringV = requires { T::fromStdString(std::string{}); };

template<typename T>
inline constexpr bool HasFromStringV = requires { T::fromString(std::string{}); };

template<typename T>
inline constexpr bool HasFreeStandFromStringV =
    requires(T* value) { fromString(std::string{}, value); };

} // namespace nx::reflect::detail
