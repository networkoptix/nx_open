// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#if (defined(_MSC_VER) && (defined(_M_AMD64) || defined(_M_X64))) || defined(__SSE2__)
    #define NX_SSE2_SUPPORTED
#endif

#if defined(NX_SSE2_SUPPORTED)
    #include <xmmintrin.h>
    #include <emmintrin.h>

    typedef __m128i simd128i;
    typedef __m128 simd128;
#elif (defined(__arm__) || defined(__aarch64__)) && defined(__ARM_NEON)
    #include <sse2neon.h>
    #define NX_SSE2_SUPPORTED
    #define NX_SSE2_SUPPORTED_SSE2NEON

    typedef __m128i simd128i;
    typedef __m128 simd128;
#else
    //#if !defined(SIMD_WARNING_ISSUED)
    //    #define SIMD_WARNING_ISSUED
    //    #warning "Target CPU has no SIMD extension or it is not supported - using C fallback routines"
    //#endif

    typedef struct
    {
        int64_t one;
        uint64_t two;
    } simd128i;

    typedef struct
    {
        uint64_t one;
        uint64_t two;
    } simd128;
#endif

#if defined(NX_SSE2_SUPPORTED)
    #if defined(Q_CC_GNU) && !defined(Q_OS_MAC) && !defined(NX_SSE2_SUPPORTED_SSE2NEON)

        /* We cannot include GCC intrinsic headers cause they cause compilation
         * errors. Instead, we place the definitions for the functions we're
         * interested in here.
         *
         * For more GCC intrinsics see:
         * http://opensource.apple.com/source/gcc/gcc-5664/gcc/config/i386/smmintrin.h
         * http://opensource.apple.com/source/gcc/gcc-5664/gcc/config/i386/tmmintrin.h
         */

        #undef __STATIC_INLINE
        #if defined(__GNUC_STDC_INLINE__)
            #define __STATIC_INLINE __inline
        #else
            #define __STATIC_INLINE static __inline
        #endif

        #define sse4_attribute __attribute__ ((__target__ ("sse4.1")))
        #define ssse3_attribute __attribute__ ((__target__ ("ssse3")))

        /* These functions will NOT be inlined with GCC because of -msse2
         * compiler option.
         * (Node "Function attributes"):
         *   On the 386/x86_64 and PowerPC backends, the inliner will not
         *   inline a function that has different target options than the
         *   caller, unless the callee has a subset of the target options of
         *   the caller.  For example a function declared with `target("sse3")'
         *   can inline a function with `target("sse2")', since `-msse3'
         *   implies `-msse2'.
         */
        __STATIC_INLINE int sse4_attribute
        _mm_testz_si128_sse4 (__m128i __M, __m128i __V)
        {
            return __builtin_ia32_ptestz128 ((__v2di)__M, (__v2di)__V);
        }

        __STATIC_INLINE __m128i sse4_attribute
        _mm_packus_epi32_sse4 (__m128i __X, __m128i __Y)
        {
          return (__m128i) __builtin_ia32_packusdw128 ((__v4si)__X, (__v4si)__Y);
        }

        __STATIC_INLINE __m128i ssse3_attribute
        _mm_hadd_epi16_ssse3 (__m128i __X, __m128i __Y)
        {
            return (__m128i) __builtin_ia32_phaddw128 ((__v8hi)__X, (__v8hi)__Y);
        }

    #else
        #if !defined(NX_SSE2_SUPPORTED_SSE2NEON)
            #include <smmintrin.h>
        #endif
        #define sse4_attribute
        #define ssse3_attribute

        #define _mm_testz_si128_sse4 _mm_testz_si128
        #define _mm_packus_epi32_sse4 _mm_packus_epi32
        #define _mm_hadd_epi16_ssse3 _mm_hadd_epi16
    #endif
#else
    #define _mm_testz_si128_sse4 _mm_testz_si128
    #define _mm_packus_epi32_sse4 _mm_packus_epi32
    #define _mm_hadd_epi16_ssse3 _mm_hadd_epi16
#endif

NX_MEDIA_CORE_API bool useSSE2();
NX_MEDIA_CORE_API bool useSSE3();
NX_MEDIA_CORE_API bool useSSSE3();
NX_MEDIA_CORE_API bool useSSE41();
NX_MEDIA_CORE_API bool useSSE42();

NX_MEDIA_CORE_API QString getCPUString();
