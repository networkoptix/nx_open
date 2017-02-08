#pragma once

namespace nx {
namespace utils {
namespace unused {
namespace detail {

inline void mark_unused() {}

template<class T0>
inline void mark_unused(const T0&) {}

template<class T0, class T1>
inline void mark_unused(const T0&, const T1&) {}

template<class T0, class T1, class T2>
inline void mark_unused(const T0&, const T1&, const T2&) {}

template<class T0, class T1, class T2, class T3>
inline void mark_unused(const T0&, const T1&, const T2&, const T3&) {}

template<class T0, class T1, class T2, class T3, class T4>
inline void mark_unused(const T0&, const T1&, const T2&, const T3&, const T4&) {}

template<class T0, class T1, class T2, class T3, class T4, class T5>
inline void mark_unused(const T0&, const T1&, const T2&, const T3&, const T4&, const T5&) {}

template<class T0, class T1, class T2, class T3, class T4, class T5, class T6>
inline void mark_unused(const T0&, const T1&, const T2&, const T3&, const T4&, const T5&, const T6&) {}

} // namespace detail
} // namespace unused
} // namespace utils
} // namespace nx

// TODO: Rename to NX_UNUSED().
/**
 * Macro to suppress unused parameter warnings. Unlike constructions like
 * <code>Q_UNUSED(X)</code> or <code>(void) x</code>, does not require arguments
 * to have a complete type. Multiple arguments are supported.
 */
#define QN_UNUSED(...) ::nx::utils::unused::detail::mark_unused(__VA_ARGS__)
