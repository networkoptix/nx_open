#ifndef __SSE_HELPER_H__
#define __SSE_HELPER_H__

#ifdef Q_OS_WIN
#define QT_HAVE_SSSE3 // Intel core and above
#include <QtCore/private/qsimd_p.h>
#endif

#ifdef Q_OS_MAC
#include <xmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>
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

#endif // __SSE_HELPER_H__
