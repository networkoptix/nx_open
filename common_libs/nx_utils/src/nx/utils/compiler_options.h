#pragma once

// -------------------------------------------------------------------------- //
// Library & Compiler globals. Do not change.
// -------------------------------------------------------------------------- //

/* Don't use deprecated Qt functions. */
// TODO: This one causes compilation errors in QObject...
//#define QT_NO_DEPRECATED

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

/* Some windows-specific defines. */
#ifdef _WIN32
#   undef NOMINMAX
#   define NOMINMAX /* We don't want min & max as macros. */
#endif


/* Some MSVC-specific defines. */
#ifdef _MSC_VER
#   undef __STDC_CONSTANT_MACROS
#   define __STDC_CONSTANT_MACROS /* We want M_PI and other math defines defined. */
#endif


/* Get rid of useless MSVC warnings. */
#ifdef _MSC_VER
#   define _CRT_SECURE_NO_WARNINGS /* Don't warn for deprecated 'unsecure' CRT functions. */
#   define _CRT_NONSTDC_NO_DEPRECATE /* Don't warn for deprecated POSIX functions. */
#   define _SCL_SECURE_NO_WARNINGS /* Don't warn for 'unsafe' STL functions. */
#
#   /* 'Derived' : inherits 'Base::method' via dominance.
#    * It is buggy as described here:
#    * http://connect.microsoft.com/VisualStudio/feedback/details/101259/disable-warning-c4250-class1-inherits-class2-member-via-dominance-when-weak-member-is-a-pure-virtual-function */
#   pragma warning(disable: 4250)
#endif


/* Turn some useful MSVC warnings into errors.
 *
 * Btw, if you intend to comment out one of the lines below, think twice. My sword is sharp. */
#ifdef _MSC_VER
#   pragma warning(error: 4150) /* deletion of pointer to incomplete type 'X'; no destructor called */
#   pragma warning(error: 4715) /* not all control paths return a value */
#   pragma warning(error: 4005) /* macro redefinition */
#   pragma warning(error: 4806) /* unsafe operation: no value of type 'INTEGRAL' promoted to type 'ENUM' can equal the given constant */
#endif

/* Compatibility for msvc2012 */
#ifdef _WIN32
#   if _MSC_VER < 1800
#       define noexcept
#   endif
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
