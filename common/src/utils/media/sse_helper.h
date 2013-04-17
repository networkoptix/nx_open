#ifndef QN_SSE_HELPER_H
#define QN_SSE_HELPER_H

#include <QtCore/private/qsimd_p.h>
#include <xmmintrin.h>
#include <emmintrin.h>


#if defined(Q_CC_MSVC)
#   include <intrin.h> /* For __cpuid. */
#elif defined(Q_CC_GNU)
#   ifdef __amd64__
#       define __cpuid(res, op)                                                 \
            __asm__ volatile(                                                   \
            "mov %%rbx, %%rsi    \n\t"                                          \
            "cpuid               \n\t"                                          \
            "mov %%rbx, %1       \n\t"                                          \
            "mov %%rsi, %%rbx    \n\t"                                          \
            :"=a"(res[0]), "=m"(res[1]), "=c"(res[2]), "=d"(res[3])             \
            :"0"(op) : "%rsi")
#   else
#       define __cpuid(res, op)                                                 \
            __asm__ volatile(                                                   \
            "movl %%ebx, %%esi   \n\t"                                          \
            "cpuid               \n\t"                                          \
            "movl %%ebx, %1      \n\t"                                          \
            "movl %%esi, %%ebx   \n\t"                                          \
            :"=a"(res[0]), "=m"(res[1]), "=c"(res[2]), "=d"(res[3])             \
            :"0"(op) : "%esi")
#   endif
#else
#   error __cpuid is not supported for your compiler.
#endif


#if defined(Q_CC_GNU) && !defined(Q_OS_MAC)

/* We cannot include GCC intrinsic headers cause they cause compilation errors.
 * Instead, we place the definitions for the functions we're interested in here.
 * 
 * For more GCC intrinsics see: 
 * http://opensource.apple.com/source/gcc/gcc-5664/gcc/config/i386/smmintrin.h
 * http://opensource.apple.com/source/gcc/gcc-5664/gcc/config/i386/tmmintrin.h
 */

#define __always_inline__ __always_inline__

#undef __STATIC_INLINE
#ifdef __GNUC_STDC_INLINE__
#define __STATIC_INLINE __inline
#else
#define __STATIC_INLINE static __inline
#endif

#define sse4_attribute __attribute__ ((__target__ ("sse4.1")))
#define ssse3_attribute __attribute__ ((__target__ ("ssse3")))

__STATIC_INLINE int __attribute__((__always_inline__)) sse4_attribute
_mm_testz_si128 (__m128i __M, __m128i __V)
{
    return __builtin_ia32_ptestz128 ((__v2di)__M, (__v2di)__V);
}

__STATIC_INLINE __m128i __attribute__((__always_inline__)) sse4_attribute
_mm_packus_epi32 (__m128i __X, __m128i __Y)
{
  return (__m128i) __builtin_ia32_packusdw128 ((__v4si)__X, (__v4si)__Y);
}

__STATIC_INLINE __m128i __attribute__((__always_inline__)) ssse3_attribute
_mm_hadd_epi16 (__m128i __X, __m128i __Y)
{
    return (__m128i) __builtin_ia32_phaddw128 ((__v8hi)__X, (__v8hi)__Y);
}

#else
#include <smmintrin.h>
#define sse4_attribute 
#define ssse3_attribute
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

// TODO: #vasilenko function too large for inlining. Move to cpp file.
static inline QString getCPUString()
{
    char CPUBrandString[0x40]; 
    int CPUInfo[4] = {-1};

    // Calling __cpuid with 0x80000000 as the InfoType argument
    // gets the number of valid extended IDs.
    __cpuid(CPUInfo, (quint32)0x80000000);
    unsigned int nExIds = CPUInfo[0];
    memset(CPUBrandString, 0, sizeof(CPUBrandString));

    // Get the information associated with each extended ID.
    for(unsigned int i=0x80000000; i<=nExIds; ++i)
    {
        __cpuid(CPUInfo, i);

        // Interpret CPU brand string and cache information.
        if  (i == 0x80000002)
            memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
        else if  (i == 0x80000003)
            memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
        else if  (i == 0x80000004)
            memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
    }

    if(nExIds >= 0x80000004)
    {
        //printf_s("\nCPU Brand String: %s\n", CPUBrandString);
        return QString::fromAscii( CPUBrandString ).trimmed();
    }
    return QString();
}

#endif // QN_SSE_HELPER_H
