// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**
 * Some c++20 features missing in Clang are defined here
 */

#if defined(__clang__)
    #if __clang_major__ < 15
        #define CLANG_MISSING_FEATURE_3WAY_COMPARE_STRING 1
    #endif

    #if __clang_major__ < 16
        #define CLANG_MISSING_FEATURE_3WAY_COMPARE_CHRONO 1
        #define CLANG_MISSING_FEATURE_3WAY_COMPARE_OPTIONAL 1
        #define CLANG_MISSING_FEATURE_3WAY_COMPARE_MAP 1
        #define CLANG_MISSING_FEATURE_3WAY_COMPARE_MULTIMAP 1
        #define CLANG_MISSING_FEATURE_3WAY_COMPARE_VECTOR 1
    #endif

    #if defined(_LIBCPP_COMPILER_CLANG_BASED) \
        || defined(__ANDROID_MIN_SDK_VERSION__) && __ANDROID_MIN_SDK_VERSION__ <= 21

        #include <algorithm>
        #include <chrono>
        #include <compare>
        #include <concepts>
        #include <cstring>
        #include <map>
        #include <optional>
        #include <string>
        #include <type_traits>

namespace std {

#if CLANG_MISSING_FEATURE_3WAY_COMPARE_STRING
template<typename CharT, typename Traits, typename Alloc>
inline std::strong_ordering operator<=>(const basic_string<CharT, Traits, Alloc>& lhs,
    const basic_string<CharT, Traits, Alloc>& rhs) noexcept
{
    auto res = std::strcmp(lhs.c_str(), rhs.c_str());
    if (res < 0)
        return std::strong_ordering::less;
    if (res > 0)
        return std::strong_ordering::greater;
    return std::strong_ordering::equal;
}
#endif // CLANG_MISSING_FEATURE_3WAY_COMPARE_STRING

#if CLANG_MISSING_FEATURE_3WAY_COMPARE_CHRONO
namespace chrono {

template<typename Rep1, typename Period1, typename Rep2, typename Period2>
constexpr auto operator<=>(
    const duration<Rep1, Period1>& lhs,
    const duration<Rep2, Period2>& rhs)
{
    using ct = common_type_t<duration<Rep1, Period1>, duration<Rep2, Period2>>;
    return ct(lhs).count() <=> ct(rhs).count();
}

template<typename Clock, typename Duration1, typename Duration2>
constexpr auto operator<=>(
    const time_point<Clock, Duration1>& lhs,
    const time_point<Clock, Duration2>& rhs)
{
    return lhs.time_since_epoch() <=> rhs.time_since_epoch();
}

} // namespace chrono
#endif // CLANG_MISSING_FEATURE_3WAY_COMPARE_CHRONO

#if CLANG_MISSING_FEATURE_3WAY_COMPARE_OPTIONAL
template <typename Tp, typename = decltype(std::declval<Tp>() <=> std::declval<Tp>())>
constexpr auto operator<=>(
    const optional<Tp>& x,
    const optional<Tp>& y)
{
    return x && y ? *x <=> *y : bool(x) <=> bool(y);
}

template <typename Tp, typename = decltype(std::declval<Tp>() <=> std::declval<Tp>())>
constexpr auto operator<=>(
    const optional<Tp>& x,
    const Tp& v)
{
    return bool(x) ? *x <=> v : strong_ordering::less;
}
#endif // CLANG_MISSING_FEATURE_3WAY_COMPARE_OPTIONAL

} // namespace std

#endif

#if defined(_LIBCPP_COMPILER_CLANG_BASED)

namespace std {

template<class I1, class I2, class Cmp>
constexpr auto lexicographical_compare_three_way(I1 f1, I1 l1, I2 f2, I2 l2, Cmp comp)
    -> decltype(comp(*f1, *f2))
{
    using ret_t = decltype(comp(*f1, *f2));
    static_assert(std::disjunction_v<std::is_same<ret_t, std::strong_ordering>,
                      std::is_same<ret_t, std::weak_ordering>,
                      std::is_same<ret_t, std::partial_ordering>>,
        "The return type must be a comparison category type.");

    bool exhaust1 = (f1 == l1);
    bool exhaust2 = (f2 == l2);
    for (; !exhaust1 && !exhaust2; exhaust1 = (++f1 == l1), exhaust2 = (++f2 == l2))
        if (auto c = comp(*f1, *f2); c != 0)
            return c;

    return !exhaust1 ? std::strong_ordering::greater
        : !exhaust2  ? std::strong_ordering::less
                     : std::strong_ordering::equal;
}

#if CLANG_MISSING_FEATURE_3WAY_COMPARE_VECTOR || \
    CLANG_MISSING_FEATURE_3WAY_COMPARE_MULTIMAP || \
    CLANG_MISSING_FEATURE_3WAY_COMPARE_MAP
namespace detail {

template<typename Tp, typename Up>
concept three_way_builtin_ptr_cmp
    = requires(Tp&& t, Up&& u)
{ static_cast<Tp&&>(t) <=> static_cast<Up&&>(u); }
    && convertible_to<Tp, const volatile void*>
    && convertible_to<Up, const volatile void*>
    && ! requires(Tp&& t, Up&& u)
{ operator<=>(static_cast<Tp&&>(t), static_cast<Up&&>(u)); }
    && ! requires(Tp&& t, Up&& u)
{ static_cast<Tp&&>(t).operator<=>(static_cast<Up&&>(u)); };

template<typename Tp, typename Up>
concept weakly_eq_cmp_with
    = requires(const remove_reference_t<Tp>& t, const remove_reference_t<Tp>& u) {
          { t == u } -> __boolean_testable;
          { t != u } -> __boolean_testable;
          { u == t } -> __boolean_testable;
          { u != t } -> __boolean_testable;
      };

template<typename Tp, typename Up>
concept partially_ordered_with
    = requires(const remove_reference_t<Tp>& t,
        const remove_reference_t<Up>& u) {
          { t <  u } -> __boolean_testable;
          { t >  u } -> __boolean_testable;
          { t <= u } -> __boolean_testable;
          { t >= u } -> __boolean_testable;
          { u <  t } -> __boolean_testable;
          { u >  t } -> __boolean_testable;
          { u <= t } -> __boolean_testable;
          { u >= t } -> __boolean_testable;
      };

template<typename Tp, typename Cat>
concept compares_as
    = same_as<common_comparison_category_t<Tp, Cat>, Cat>;

template<typename Tp, typename Cat = partial_ordering>
concept three_way_comparable
    = weakly_eq_cmp_with<Tp, Tp>
    && partially_ordered_with<Tp, Tp>
    && requires(const remove_reference_t<Tp>& a,
        const remove_reference_t<Tp>& b)
{
    { a <=> b } -> compares_as<Cat>;
};

template<typename Tp, typename Up, typename Cat = partial_ordering>
concept three_way_comparable_with
    = three_way_comparable<Tp, Cat>
    && three_way_comparable<Up, Cat>
    && common_reference_with<const remove_reference_t<Tp>&,
    const remove_reference_t<Up>&>
    && three_way_comparable<
        common_reference_t<const remove_reference_t<Tp>&,
            const remove_reference_t<Up>&>, Cat>
        && weakly_eq_cmp_with<Tp, Up>
            && partially_ordered_with<Tp, Up>
                && requires(const remove_reference_t<Tp>& t,
            const remove_reference_t<Up>& u)
{
    { t <=> u } -> compares_as<Cat>;
    { u <=> t } -> compares_as<Cat>;
};

inline constexpr struct Synth3way
{
    template<typename Tp, typename Up>
    static constexpr bool
        S_noexcept(const Tp* t = nullptr, const Up* u = nullptr)
    {
        if constexpr (three_way_comparable_with<Tp, Up>)
            return noexcept(*t <=> *u);
        else
            return noexcept(*t < *u) && noexcept(*u < *t);
    }

    template<typename Tp, typename Up>
    [[nodiscard]]
    constexpr auto
        operator()(const Tp& t, const Up& u) const
        noexcept(S_noexcept<Tp, Up>())
        requires requires
    {
        { t < u } -> __boolean_testable;
        { u < t } -> __boolean_testable;
    }
    {
        if constexpr (three_way_comparable_with<Tp, Up>)
            return t <=> u;
        else
        {
            if (t < u)
                return weak_ordering::less;
            else if (u < t)
                return weak_ordering::greater;
            else
                return weak_ordering::equivalent;
        }
    }
} synth3way = {};

template<typename Tp, typename Up = Tp>
using synth3way_t
    = decltype(detail::synth3way(std::declval<Tp&>(),
        std::declval<Up&>()));

} // namespace detail

struct compare_three_way
{
    template<typename Tp, typename Up>
    constexpr auto operator() [[nodiscard]] (Tp&& t, Up&& u) const
    {
        if constexpr (detail::three_way_builtin_ptr_cmp<Tp, Up>)
        {
            auto pt = static_cast<const volatile void*>(t);
            auto pu = static_cast<const volatile void*>(u);
            if (std::is_constant_evaluated())
                return pt <=> pu;
            auto it = reinterpret_cast<__UINTPTR_TYPE__>(pt);
            auto iu = reinterpret_cast<__UINTPTR_TYPE__>(pu);
            return it <=> iu;
        }
        else
            return static_cast<Tp&&>(t) <=> static_cast<Up&&>(u);
    }

    using is_transparent = void;
};
#endif

#if CLANG_MISSING_FEATURE_3WAY_COMPARE_MAP
template<typename Key, typename Tp, typename Compare, typename Alloc>
constexpr detail::synth3way_t<pair<const Key, Tp>> operator<=>(
    const std::map<Key, Tp, Compare, Alloc>& x, const std::map<Key, Tp, Compare, Alloc>& y)
{
    return std::lexicographical_compare_three_way(
        x.begin(), x.end(),
        y.begin(), y.end(),
        detail::synth3way);
}
#endif // CLANG_MISSING_FEATURE_3WAY_COMPARE_MAP

#if CLANG_MISSING_FEATURE_3WAY_COMPARE_MULTIMAP
template<typename Key, typename Tp, typename Compare, typename Alloc>
constexpr detail::synth3way_t<pair<const Key, Tp>> operator<=>(
    const std::multimap<Key, Tp, Compare, Alloc>& x, const std::multimap<Key, Tp, Compare, Alloc>& y)
{
    return std::lexicographical_compare_three_way(
        x.begin(), x.end(),
        y.begin(), y.end(),
        detail::synth3way);
}
#endif // CLANG_MISSING_FEATURE_3WAY_COMPARE_MULTIMAP

#if CLANG_MISSING_FEATURE_3WAY_COMPARE_VECTOR
template<typename Tp, typename Alloc>
inline detail::synth3way_t<Tp> operator<=>(
    const vector<Tp, Alloc>& x,
    const vector<Tp, Alloc>& y)
{
    return std::lexicographical_compare_three_way(
        x.begin(),
        x.end(),
        y.begin(),
        y.end(),
        detail::synth3way);
}
#endif // CLANG_MISSING_FEATURE_3WAY_COMPARE_VECTOR

} // namespace std

#endif // defined(_LIBCPP_COMPILER_CLANG_BASED)

#endif // defined(__clang__) && (__clang_major__ < kClangMissingCpp20SupportVersion)
