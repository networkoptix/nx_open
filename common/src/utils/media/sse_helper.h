#ifndef QN_SSE_HELPER_H
#define QN_SSE_HELPER_H

#include <QtCore/private/qsimd_p.h>
#include <xmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>

#if defined(Q_CC_GNU)
#define sse4_attribute __attribute__ ((__target__ ("sse4.1")))
#else
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
