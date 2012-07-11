#ifndef QN_SSE_HELPER_H
#define QN_SSE_HELPER_H

#include <QtCore/private/qsimd_p.h>
#include <xmmintrin.h>
#include <emmintrin.h>


#if defined(Q_CC_GNU)

// it is required to fix <smmintrin.h> GCC header.
// For each function need to add 'sse4_attribute'
// For more internal GCC SSE4 intrisicts see: http://opensource.apple.com/source/gcc/gcc-5664/gcc/config/i386/smmintrin.h

#define __always_inline__ __always_inline__, __nodebug__

#undef __STATIC_INLINE
#ifdef __GNUC_STDC_INLINE__
#define __STATIC_INLINE __inline
#else
#define __STATIC_INLINE static __inline
#endif

#define sse4_attribute __attribute__ ((__target__ ("sse4.1")))

#ifndef Q_OS_MAC // Mac gcc smmintrin.h header already has this
__STATIC_INLINE int __attribute__((__always_inline__)) sse4_attribute
_mm_testz_si128 (__m128i __M, __m128i __V)
{
    return __builtin_ia32_ptestz128 ((__v2di)__M, (__v2di)__V);
}

__STATIC_INLINE __m128i __attribute__((__gnu_inline__, __always_inline__, __artificial__)) sse4_attribute
_mm_packus_epi32 (__m128i __X, __m128i __Y)
{
  return (__m128i) __builtin_ia32_packusdw128 ((__v4si)__X, (__v4si)__Y);
}

#endif

#else
#include <smmintrin.h>
#define sse4_attribute 
#endif


static inline bool useSSE2()
{
#ifdef Q_OS_MAC
    return true;
#else
    return qDetectCPUFeatures() & SSE2;
#endif
}

static inline bool useSSE3()
{
#ifdef Q_OS_MAC
    return true;
#else
    return qDetectCPUFeatures() & SSE3;
#endif
}

static inline bool useSSSE3()
{
#ifdef Q_OS_MAC
    return true;
#else
    return qDetectCPUFeatures() & SSSE3;
#endif
}

static inline bool useSSE41()
{
#ifdef Q_OS_MAC
    return true;
#else
    return qDetectCPUFeatures() & SSE4_1;
#endif
}

static inline bool useSSE42()
{
#ifdef Q_OS_MAC
    return true;
#else
    return qDetectCPUFeatures() & SSE4_2;
#endif
}

#endif // QN_SSE_HELPER_H
