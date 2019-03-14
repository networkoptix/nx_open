#pragma once

namespace nx {
namespace utils {
namespace meta {

template <typename F>
struct SignatureExtractor;

template<typename Return, typename Class, typename... Args>
struct SignatureExtractor<Return (Class::*)(Args...)>
{
    using type = Return (Args...);
};

template<typename Return, typename Class, typename... Args>
struct SignatureExtractor<Return (Class::*)(Args...) const>
{
    using type = Return (Args...);
};

} // namespace meta
} // namespace utils
} // namespace nx
