// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef __YUV_CONVERT_H
#define __YUV_CONVERT_H

#include <QtCore/QtGlobal>

static const unsigned int X_STRIDE_FOR_SSE_CONVERT_UTILS = 32;

// All the following functions use SSE on x86, but have stub implementations for ARMs, thus, can be
// compiled on any platform but should be called only on x64.

/** ATTENTION: Despite its name, this function actually converts to BGRA. */
NX_MEDIA_CORE_API void yuv444_argb32_simd_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha);

/** ATTENTION: Despite its name, this function actually converts to BGRA. */
NX_MEDIA_CORE_API void yuv422_argb32_simd_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha);

/** ATTENTION: Despite its name, this function actually converts to BGRA. */
NX_MEDIA_CORE_API void yuv420_argb32_simd_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha);

NX_MEDIA_CORE_API void bgra_yuv420(
    quint8* rgba, quint8* yptr, quint8* uptr, quint8* vptr, int width, int height, bool flip);
/*!
    \param xStride Line length in \a rgba buffer. MUST be multiple of 32
    \param yStride Line length in \a y
    \param uvStride Line length in \a u and \a v buffers
*/
NX_MEDIA_CORE_API void bgra_to_yv12_simd_intr(
    const quint8* rgba, int xStride, quint8* y, quint8* u, quint8* v, int yStride,
    int uvStride, int width, int height, bool flip);
//!Converts bgra to yuv420 with alpha plane, total 20 bits per pixel (Y - 8bit, A - 8bit, UV)
/*!
    \param xStride Line length in \a rgba buffer. MUST be multiple of 32
    \param yStride Line length in \a y
    \param uvStride Line length in \a u and \a v buffers
*/
NX_MEDIA_CORE_API void bgra_to_yva12_simd_intr(
    const quint8* rgba, int xStride, quint8* y, quint8* u, quint8* v, quint8* a,
    int yStride, int uvStride, int aStride, int width, int height, bool flip);


#endif
