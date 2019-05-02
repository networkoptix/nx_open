#pragma once

#include <memory>
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

//-------------------------------------------------------------------------------------------------

template<typename Tuple, std::size_t index, typename Func>
static inline void tuple_for_each(const Tuple& tuple, Func func)
{
    func(std::get<index>(tuple));

    if constexpr (std::tuple_size<Tuple>::value > index + 1)
        tuple_for_each<Tuple, index + 1>(tuple, func);
}

//-------------------------------------------------------------------------------------------------

template<typename Func, typename FirstArg, typename... Args>
inline void apply_each(Func&& func, FirstArg&& firstArg, Args&& ... args)
{
    func(std::forward<FirstArg>(firstArg));

    if constexpr (sizeof...(args) > 0)
        apply_each(std::forward<Func>(func), std::forward<Args>(args)...);
}

} // namespace detail

/** Calls f passing it members of tuple t as arguments. */
template<typename F, typename T>
inline auto expandTupleIntoArgs(F && f, T && t)->
decltype(detail::Apply<
    ::std::tuple_size<typename ::std::decay<T>::type>::value>::apply(
        ::std::forward<F>(f), ::std::forward<T>(t)))
{
    return detail::Apply<::std::tuple_size<typename ::std::decay<T>::type>::value>
        ::apply(::std::forward<F>(f), ::std::forward<T>(t));
}

/**
 * NOTE: Tuple elements are interated forward.
 */
template<typename Tuple, typename Func>
inline void tuple_for_each(const Tuple& tuple, Func func)
{
    detail::tuple_for_each<Tuple, 0>(tuple, func);
}

/**
 * Invokes func with each argument of Args pack separately.
 * NOTE: Args packs elements are interated forward.
 */
template<typename Func, typename... Args>
inline void apply_each(Func&& func, Args&&... args)
{
    if constexpr(sizeof...(args) > 0)
        detail::apply_each(std::forward<Func>(func), std::forward<Args>(args)...);
}

/**
 * Converts unique_ptr of one type to another using static_cast.
 * Consumes original unique_ptr. Actually moves memory to a new unique_ptr.
 */
template<typename ResultType, typename InitialType, typename DeleterType>
std::unique_ptr<ResultType, DeleterType>
static_unique_ptr_cast(std::unique_ptr<InitialType, DeleterType>&& sourcePtr)
{
    return std::unique_ptr<ResultType, DeleterType>(
        static_cast<ResultType>(sourcePtr.release()),
        sourcePtr.get_deleter());
}

template<typename ResultType, typename InitialType>
std::unique_ptr<ResultType, std::default_delete<ResultType>>
static_unique_ptr_cast(
    std::unique_ptr<InitialType, std::default_delete<InitialType>>&& sourcePtr)
{
    return std::unique_ptr<ResultType>(
        static_cast<ResultType*>(sourcePtr.release()));
}

template<typename T, typename Type = void>
using SfinaeCheck = Type;

template<typename Object, typename Deleter>
std::unique_ptr<Object, Deleter> wrapUnique(Object* ptr, Deleter deleter)
{
    return std::unique_ptr<Object, Deleter>(ptr, std::move(deleter));
}

template<typename Object, typename Deleter>
std::shared_ptr<Object> wrapShared(Object* ptr, Deleter deleter)
{
    return std::shared_ptr<Object>(ptr, std::move(deleter));
}


/**
 * tuple_first_element<default_type, tuple_type>.
 * tuple_first_element::type is typedefed to default_type if tuple_type is empty.
 * Otherwise, to the type of tuple's first element.
 */
template<typename DefaultType, typename T> struct tuple_first_element;

template<typename DefaultType, typename Head, typename... Args>
struct tuple_first_element<DefaultType, std::tuple<Head, Args...>>
{
    typedef Head type;
};

template<typename DefaultType>
struct tuple_first_element<DefaultType, std::tuple<>>
{
    typedef DefaultType type;
};

template<typename T>
struct identity
{
    using type = T;
};

} // namespace utils
} // namespace nx
