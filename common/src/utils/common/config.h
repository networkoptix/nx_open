#ifndef QN_COMMON_CONFIG_H
#define QN_COMMON_CONFIG_H

// -------------------------------------------------------------------------- //
// Globals. Do not change.
// -------------------------------------------------------------------------- //
/* Get GCC version in human-readable format. */
#ifdef __GNUC__
#  define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif


/* Use expression-template-based string concatenation in Qt. */
#define QT_USE_FAST_CONCATENATION
#define QT_USE_FAST_OPERATOR_PLUS


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
#  if (GCC_VERSION >= 406000)
#    define noexcept noexcept
#  else
#    define noexcept throw()
#  endif
#else
#  define noexcept
#endif



#endif // QN_COMMON_CONFIG_H
