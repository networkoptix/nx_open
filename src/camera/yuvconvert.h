#ifndef __YUV_CONVERT_H
#define __YUV_CONVERT_H

void yuv420_argb32_mmx(unsigned char * image, const unsigned char * py,
			      const unsigned char * pu, const unsigned char * pv,
			      const unsigned int h_size, const unsigned int v_size,
			      const unsigned int image_stride, const unsigned int y_stride,
			      const unsigned int uv_stride);

void yuv422_argb32_mmx(unsigned char * image, const unsigned char * py,
			      const unsigned char * pu, const unsigned char * pv,
			      const unsigned int h_size, const unsigned int v_size,
			      const unsigned int image_stride, const unsigned int y_stride,
			      const unsigned int uv_stride);

void yuv444_argb32_mmx(unsigned char * image, const unsigned char * py,
			      const unsigned char * pu, const unsigned char * pv,
			      const unsigned int h_size, const unsigned int v_size,
			      const unsigned int image_stride, const unsigned int y_stride,
			      const unsigned int uv_stride);


#endif
