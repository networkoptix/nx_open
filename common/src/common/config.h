#pragma once

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
