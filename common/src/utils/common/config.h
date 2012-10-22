#ifndef QN_COMMON_CONFIG_H
#define QN_COMMON_CONFIG_H

// -------------------------------------------------------------------------- //
// Application globals. Do not change.
// -------------------------------------------------------------------------- //

// TODO: #VASILENKO add doxy-comments for these defines
#define CL_MEDIA_ALIGNMENT 32
#define CL_MEDIA_EXTRA 8


// -------------------------------------------------------------------------- //
// Library & Compiler globals. Do not change.
// -------------------------------------------------------------------------- //
/* Get GCC version in human-readable format. */
#ifdef __GNUC__
#  define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif


/* Use expression-template-based string concatenation in Qt. */
//#define QT_USE_FAST_CONCATENATION
//#define QT_USE_FAST_OPERATOR_PLUS


/* Define override specifier for MSVC. */
#ifdef _MSC_VER
#  define override override
#else
#  define override
#endif


/* Define noexcept. */
#if defined(_MSC_VER)
#  define noexcept throw()
#elif defined(__GNUC__)
#  if (GCC_VERSION >= 40600)
#    define noexcept noexcept
#  else
#    define noexcept throw()
#  endif
#else
#  define noexcept
#endif


/* Some windows-specific defines. */
#ifdef _WIN32
#   undef NOMINMAX
#   define NOMINMAX 
#endif


/* Some MSVC-specific defines. */
#ifdef _MSC_VER
#   undef __STDC_CONSTANT_MACROS
#   define __STDC_CONSTANT_MACROS
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


/* Helper function that can be used to 'place' macros into Qn namespace. */
namespace Qn {
    template<class T>
    const T &_id(const T &value) { return value; }
}


#endif // QN_COMMON_CONFIG_H
