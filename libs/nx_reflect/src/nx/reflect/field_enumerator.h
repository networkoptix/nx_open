// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <string_view>

namespace nx::reflect {

/**
 * @return list of type's fields' names.
 */
template<typename T>
static constexpr auto listFieldNames()
{
    return nx::reflect::visitAllFields<T>(
        [](auto&&... fields) { return std::array{std::string_view{fields.name()}...}; });
}

} // namespace nx::reflect
