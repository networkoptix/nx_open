/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <cuda_runtime_api.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <cuda.h>
#include <cuda_runtime.h>

#include "cudaEGL.h"
#include "NvAnalysis.h"

#include <iostream>

#include "NvCudaProc.h"

static void
Handle_EGLImage(EGLImageKHR image);

void
HandleEGLImage(void *pEGLImage)
{
    EGLImageKHR *pImage = (EGLImageKHR *)pEGLImage;
    Handle_EGLImage(*pImage);
}


/**
  * Performs CUDA Operations on egl image.
  *
  * @param image : EGL image
  */
static void
Handle_EGLImage(EGLImageKHR image)
{
    CUresult status;
    CUeglFrame eglFrame;
    CUgraphicsResource pResource = NULL;

    cudaFree(0);
    status = cuGraphicsEGLRegisterImage(&pResource, image,
                CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE);
    if (status != CUDA_SUCCESS)
    {
        printf("cuGraphicsEGLRegisterImage failed: %d, cuda process stop\n",
                        status);
        return;
    }

    status = cuGraphicsResourceGetMappedEglFrame(&eglFrame, pResource, 0, 0);
    if (status != CUDA_SUCCESS)
    {
        printf("cuGraphicsSubResourceGetMappedArray failed\n");
    }

    status = cuCtxSynchronize();
    if (status != CUDA_SUCCESS)
    {
        printf("cuCtxSynchronize failed\n");
    }

    if (eglFrame.frameType == CU_EGL_FRAME_TYPE_PITCH)
    {
        //Rect label in plan Y, you can replace this with any cuda algorithms.
        addLabels((CUdeviceptr) eglFrame.frame.pPitch[0], eglFrame.pitch);
    }

    status = cuCtxSynchronize();
    if (status != CUDA_SUCCESS)
    {
        printf("cuCtxSynchronize failed after memcpy\n");
    }

    status = cuGraphicsUnregisterResource(pResource);
    if (status != CUDA_SUCCESS)
    {
        printf("cuGraphicsEGLUnRegisterResource failed: %d\n", status);
    }
}

/**
  * Performs map egl image into cuda memory.
  *
  * @param pEGLImage: EGL image
  * @param width: Image width
  * @param height: Image height
  * @param cuda_buf: destnation cuda address
  */
void mapEGLImage2Float(void* pEGLImage, int width, int height, void* cuda_buf)
{
    CUresult status;
    CUeglFrame eglFrame;
    CUgraphicsResource pResource = NULL;
    EGLImageKHR *pImage = (EGLImageKHR *)pEGLImage;

    cudaFree(0);
    status = cuGraphicsEGLRegisterImage(&pResource, *pImage,
                CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE);
    if (status != CUDA_SUCCESS)
    {
        printf("cuGraphicsEGLRegisterImage failed: %d, cuda process stop\n",
                        status);
        return;
    }

    status = cuGraphicsResourceGetMappedEglFrame(&eglFrame, pResource, 0, 0);
    if (status != CUDA_SUCCESS)
    {
        printf("cuGraphicsSubResourceGetMappedArray failed\n");
    }

    status = cuCtxSynchronize();
    if (status != CUDA_SUCCESS)
    {
        printf("cuCtxSynchronize failed\n");
    }

    if (eglFrame.frameType == CU_EGL_FRAME_TYPE_PITCH)
    {
        //using GPU to convert int buffer into float buffer.
        convertIntToFloat((CUdeviceptr) eglFrame.frame.pPitch[0],
                            width, height, cuda_buf, eglFrame.pitch);
    }

    status = cuCtxSynchronize();
    if (status != CUDA_SUCCESS)
    {
        printf("cuCtxSynchronize failed after memcpy\n");
    }

    status = cuGraphicsUnregisterResource(pResource);
    if (status != CUDA_SUCCESS)
    {
        printf("cuGraphicsEGLUnRegisterResource failed: %d\n", status);
    }
}

void mapEGLImage2Float2(void* pEGLImage, int width, int height, void* cuda_buf)
{
    CUresult status;
    CUeglFrame eglFrame;
    CUgraphicsResource pResource = NULL;
    EGLImageKHR *pImage = (EGLImageKHR *)pEGLImage;

    cudaFree(0);
    status = cuGraphicsEGLRegisterImage(&pResource, *pImage,
                CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE);
    if (status != CUDA_SUCCESS)
    {
        printf("cuGraphicsEGLRegisterImage failed: %d, cuda process stop\n",
                        status);
        return;
    }

    status = cuGraphicsResourceGetMappedEglFrame(&eglFrame, pResource, 0, 0);
    if (status != CUDA_SUCCESS)
    {
        printf("cuGraphicsSubResourceGetMappedArray failed\n");
    }

    status = cuCtxSynchronize();
    if (status != CUDA_SUCCESS)
    {
        printf("cuCtxSynchronize failed\n");
    }

    if (eglFrame.frameType == CU_EGL_FRAME_TYPE_PITCH)
    {
        //using GPU to convert int buffer into float buffer.
        convertIntToFloatWithMean(
            (int*) eglFrame.frame.pPitch[0],
            width,
            height,
            cuda_buf,
            eglFrame.pitch,
            make_float3(104.0069879317889f, 116.66876761696767f, 122.6789143406786f));
    }

    status = cuCtxSynchronize();
    if (status != CUDA_SUCCESS)
    {
        printf("cuCtxSynchronize failed after memcpy\n");
    }

    status = cuGraphicsUnregisterResource(pResource);
    if (status != CUDA_SUCCESS)
    {
        printf("cuGraphicsEGLUnRegisterResource failed: %d\n", status);
    }
}

void mapIntRgbaToFloatBgr(int* rgba, int rgbaSize, void* cudaBuf)
{
    std::cout << "MAPPING INT RGBA TO FLOAT BGR WITH CUDA" << std::endl;

    void* devicePtr;
    cudaMalloc(&devicePtr, rgbaSize * sizeof(int));
    cudaMemcpy(
        devicePtr,
        (void*)rgba,
        rgbaSize * sizeof(int),
        cudaMemcpyHostToDevice);

    convertIntToFloatWithMean(
        rgba,
        1024,
        512,
        cudaBuf,
        1024,
        make_float3(104.0069879317889f, 116.66876761696767f, 122.6789143406786f));

    cudaFree(devicePtr);

}

