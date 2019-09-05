#pragma once

#include <type_traits>

namespace nx::utils {

template<typename T, typename = void>
struct isIterable: std::false_type {};

template<typename T>
struct isIterable<
    T,
    std::void_t<
        decltype(std::declval<T>().begin()),
        decltype(std::declval<T>().end())
    >
>: std::true_type {};

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

namespace std {

#if defined(ANDROID) || defined(__ANDROID__)

template<class F, class... ArgTypes>
struct invoke_result
{
    using type = typename std::result_of<F(ArgTypes...)>::type;
};

template<class F, class... ArgTypes>
using invoke_result_t = typename invoke_result<F, ArgTypes...>::type;

#endif

} // namespace std
