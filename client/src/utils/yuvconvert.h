#ifndef __YUV_CONVERT_H
#define __YUV_CONVERT_H

void yuv444_argb32_sse2_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha);
void yuv422_argb32_sse2_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha);
void yuv420_argb32_sse2_intr(unsigned char * dst, const unsigned char * py,
                            const unsigned char * pu, const unsigned char * pv,
                            const unsigned int width, const unsigned int height,
                            const unsigned int dst_stride, const unsigned int y_stride,
                            const unsigned int uv_stride, quint8 alpha);

void bgra_yuv420(quint8* rgba, quint8* yptr, quint8* uptr, quint8* vptr, int width, int height, bool flip);
void bgra_to_yv12_sse2_intr(quint8* rgba, int xStride, quint8* y, quint8* u, quint8* v, int yStride, int uvStride, int width, int height, bool flip);


#endif
