#ifndef QN_COMMON_CONFIG_H
#define QN_COMMON_CONFIG_H

// -------------------------------------------------------------------------- //
// Application globals. Do not change.
// -------------------------------------------------------------------------- //

/** Media data alignment. We use 32 for compatibility with AVX instruction set */
#define CL_MEDIA_ALIGNMENT 32

/** Additional free space at a end of memory block. Some ffmpeg calls requires it */
#define CL_MEDIA_EXTRA 8

#define CL_MAX_CHANNELS 4 // TODO: #Elric get rid of this definition

//TODO: #GDM think about correct place
static const int DEFAULT_RESOURCE_INIT_THREADS_COUNT = 64;

// -------------------------------------------------------------------------- //
// Library & Compiler globals. Do not change.
// -------------------------------------------------------------------------- //
/* Get GCC version in human-readable format. */
#ifdef __GNUC__
#   define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif


/* Use expression-template-based string concatenation in Qt. */
//#define QT_USE_FAST_CONCATENATION
//#define QT_USE_FAST_OPERATOR_PLUS


/* Don't use deprecated Qt functions. */
// TODO: This one causes compilation errors in QObject...
//#define QT_NO_DEPRECATED


/* Use variadic macros in boost even for older GCC versions. */
#if !defined(BOOST_PP_VARIADICS) && defined(__GNUC__) && GCC_VERSION >= 40000
#   define BOOST_PP_VARIADICS
#endif


/* Define override specifier. */
#if defined(_MSC_VER)
#   define override override
#elif defined(__GNUC__) && GCC_VERSION < 40700
#   define override
#else
#   define override
#endif


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


/* Define foreach */
//#define foreach BOOST_FOREACH


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


#endif // QN_COMMON_CONFIG_H
