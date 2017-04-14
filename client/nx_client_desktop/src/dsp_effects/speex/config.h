// Microsoft version of 'inline'

#include "stdio.h"

#define inline __inline

// Visual Studio support alloca(), but it always align variables to 16-bit
// boundary, while SSE need 128-bit alignment. So we disable alloca() when
// SSE is enabled.
#ifndef _USE_SSE
#  define USE_ALLOCA
#endif

/* Default to floating point */
#ifndef FIXED_POINT
#  define FLOATING_POINT
#  define USE_SMALLFT
#else
#  define USE_KISS_FFT
#endif

/* We don't support visibility on Win32 */
#define EXPORT

static inline void speex_warning(const char *str)
{
#ifndef DISABLE_WARNINGS
    fprintf (stderr, "warning: %s\n", str);
#endif
} 

static inline void speex_warning_int(const char *str, int val)
{
#ifndef DISABLE_WARNINGS
    fprintf (stderr, "warning: %s %d\n", str, val);
#endif
} 

#ifdef _MSC_VER
#    pragma warning(disable: 4244) /* C4244: '?' : conversion from '?' to '?', possible loss of data. */
#    pragma warning(disable: 4305) /* C4305: '?' : truncation from '?' to '?'. */
#endif
