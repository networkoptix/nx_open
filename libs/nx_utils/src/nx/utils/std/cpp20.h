// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**
 * Some c++20 features missing in Clang are defined here
 */
#if defined(__clang__) && defined(_LIBCPP_COMPILER_CLANG_BASED)

#include <compare>
#include <cstring>
#include <string>

namespace std {

template<typename CharT, typename Traits, typename Alloc>
inline std::strong_ordering operator<=>(const basic_string<CharT, Traits, Alloc>& lhs,
    const basic_string<CharT, Traits, Alloc>& rhs) noexcept
{
    return std::strcmp(lhs.c_str(), rhs.c_str());
}

} // namespace std

#endif // defined(__clang__) && defined(_LIBCPP_COMPILER_CLANG_BASED)
