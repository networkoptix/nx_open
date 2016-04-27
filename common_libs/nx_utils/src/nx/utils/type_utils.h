/**********************************************************
* Apr 27, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <tuple>
#include <type_traits>
#include <utility>


namespace nx {
namespace utils {

namespace detail {

template<size_t N>
struct Apply
{
    template<typename F, typename T, typename... A>
    static inline auto apply(F && f, T && t, A &&... a)
        -> decltype(Apply<N - 1>::apply(
            ::std::forward<F>(f), ::std::forward<T>(t),
            ::std::get<N - 1>(::std::forward<T>(t)), ::std::forward<A>(a)...
            ))
    {
        return Apply<N - 1>::apply(::std::forward<F>(f), ::std::forward<T>(t),
            ::std::get<N - 1>(::std::forward<T>(t)), ::std::forward<A>(a)...
            );
    }
};

template<>
struct Apply<0>
{
    template<typename F, typename T, typename... A>
    static inline auto apply(F && f, T &&, A &&... a)
        -> decltype(::std::forward<F>(f)(::std::forward<A>(a)...))
    {
        return ::std::forward<F>(f)(::std::forward<A>(a)...);
    }
};

}   //namespace detail

/** Calls \a f passing it members of tuple \a t as arguments */
template<typename F, typename T>
inline auto expandTupleIntoArgs(F && f, T && t)->
decltype(detail::Apply<
    ::std::tuple_size<typename ::std::decay<T>::type>::value>::apply(
        ::std::forward<F>(f), ::std::forward<T>(t)))
{
    return detail::Apply<::std::tuple_size<typename ::std::decay<T>::type>::value>
        ::apply(::std::forward<F>(f), ::std::forward<T>(t));
}

}   //namespace utils
}   //namespace nx
