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

#endif
