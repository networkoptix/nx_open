#ifndef __SSE_HELPER_H__
#define __SSE_HELPER_H__

#ifdef Q_OS_WIN
#define QT_HAVE_SSSE3 // Intel core and above
#include <QtCore/private/qsimd_p.h>
#else
#include "qsimd_p.h"
#endif

#include <xmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>

static inline bool useSSE2()
{
#ifdef Q_OS_MAC
    return true;
#else
    return qnDetectCPUFeatures() & SSE2;
#endif
}

static inline bool useSSE3()
{
#ifdef Q_OS_MAC
    return true;
#else
    return qnDetectCPUFeatures() & SSE3;
#endif
}

static inline bool useSSSE3()
{
#ifdef Q_OS_MAC
    return true;
#else
    return qnDetectCPUFeatures() & SSSE3;
#endif
}

static inline bool useSSE41()
{
#ifdef Q_OS_MAC
    return true;
#else
    return qnDetectCPUFeatures() & SSE4_1;
#endif
}

static inline bool useSSE42()
{
#ifdef Q_OS_MAC
    return true;
#else
    return qnDetectCPUFeatures() & SSE4_2;
#endif
}

#endif // __SSE_HELPER_H__
