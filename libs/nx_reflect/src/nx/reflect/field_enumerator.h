// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <string_view>

namespace nx::reflect {

namespace detail {

struct Enumerator
{
    template<typename... Fields>
    constexpr auto operator()(Fields... fields) -> std::array<std::string_view, sizeof...(Fields)>
    {
        return std::array<std::string_view, sizeof...(Fields)>{fields.name()...};
    }
};

} // namespace detail

//-------------------------------------------------------------------------------------------------

/**
 * @return list of type's fields' names.
 */
template<typename T>
static constexpr auto listFieldNames()
{
    return nx::reflect::visitAllFields<T>(detail::Enumerator());
}

} // namespace nx::reflect
