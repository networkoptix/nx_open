/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
* @file nvbuf_utils.h
* @brief NVBUF Utils library to be used by applications
*/

#ifndef _NVBUF_UTILS_H_
#define _NVBUF_UTILS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <errno.h>

#define MAX_NUM_PLANES 3

typedef enum
{
  NvBufferLayout_Pitch,
  NvBufferLayout_BlockLinear,
} NvBufferLayout;

typedef enum
{
  NvBufferColorFormat_YUV420,
  NvBufferColorFormat_YVU420,
  NvBufferColorFormat_NV12,
  NvBufferColorFormat_NV21,
  NvBufferColorFormat_UYVY,
  NvBufferColorFormat_ABGR32,
  NvBufferColorFormat_XRGB32,
  NvBufferColorFormat_Invalid,
} NvBufferColorFormat;

typedef struct _NvBufferParams
{
  uint32_t dmabuf_fd;
  void *nv_buffer;
  uint32_t nv_buffer_size;
  uint32_t pixel_format;
  uint32_t num_planes;
  uint32_t width[MAX_NUM_PLANES];
  uint32_t height[MAX_NUM_PLANES];
  uint32_t pitch[MAX_NUM_PLANES];
  uint32_t offset[MAX_NUM_PLANES];
}NvBufferParams;

/**
* This method should be used for getting EGLImage from dmabuf-fd
* @param[in] display EGLDisplay object used during creation of EGLImage
* @param[in] dmabuf_fd DMABUF FD of buffer from which EGLImage to be created
*
* @returns EGLImageKHR for success, NULL for failure
*/
EGLImageKHR NvEGLImageFromFd (EGLDisplay display, int dmabuf_fd);

/**
* This method should be used for destroying EGLImage object
* @param[in] display EGLDisplay object used for destroying EGLImage
* @param[in] eglImage EGLImageKHR object to be destroyed
*
* @returns 0 for success, -1 for failure
*/
int NvDestroyEGLImage (EGLDisplay display, EGLImageKHR eglImage);

/**
 * This method can be used to allocate hw buffer.
 * @param[out] dmabuf_fd returns dmabuf_fd of hardware buffer
 * @param[in] width hardware buffer width in bytes
 * @param[in] height hardware buffer height in bytes
 * @param[in] layout layout of buffer
 * @param[in] colorFormat colorFormat of buffer
 *
 * @returns 0 for success, -1 for failure
 */
int NvBufferCreate (int *dmabuf_fd, int width, int height,
    NvBufferLayout layout, NvBufferColorFormat colorFormat);

/**
 * This method can be used to get buffer parameters.
 * @param[in] dmabuf_fd DMABUF FD of buffer
 * @param[out] params structure which will be filled with parameters.
 *
 * @returns 0 for success, -1 for failure.
 */
int NvBufferGetParams (int dmabuf_fd, NvBufferParams *params);

/**
* This method should be used for destroying hw_buffer
* @param[in] dmabuf_fd dmabuf_fd hw_buffer to be destroyed
*
* @returns 0 for success, -1 for failure
*/
int NvBufferDestroy (int dmabuf_fd);

#ifdef __cplusplus
}
#endif

#endif
