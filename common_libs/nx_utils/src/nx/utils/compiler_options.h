#pragma once

// -------------------------------------------------------------------------- //
// Library & Compiler globals. Do not change.
// -------------------------------------------------------------------------- //

/* Define NULL. */
#ifdef __cplusplus
#   undef NULL
#   define NULL nullptr
#
#   ifdef __GNUC__
#       undef __null
#       define __null nullptr
#   endif
#endif

#if defined(__cplusplus) && defined(__clang__)
    #include <cstddef>
    using ::std::nullptr_t;
#endif

// -------------------------------------------------------------------------- //
// Useful utility definitions
// -------------------------------------------------------------------------- //
/* Macros to avoid using #ifdef _DEBUG. */
#ifdef _DEBUG
#   define DEBUG_CODE(...) __VA_ARGS__
#   define RELEASE_CODE(...)
#else
#   define DEBUG_CODE(...)
#   define RELEASE_CODE(...) __VA_ARGS__
#endif

//
#ifndef BOOST_BIND_NO_PLACEHOLDERS
#define BOOST_BIND_NO_PLACEHOLDERS
#endif // BOOST_BIND_NO_PLACEHOLDERS
