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
#include <stdlib.h>
#include <string.h>
#include <Argus/Argus.h>

#include "Error.h"
#include "UniquePointer.h"

#include <cuda.h>
#include <cudaEGL.h>

#include "CUDAHelper.h"
#include "histogram.h"

using namespace Argus;

namespace ArgusSamples
{

// Constants
static const Size     STREAM_SIZE (640,480);
static const uint32_t FRAME_COUNT = 10;

// Global variables
CUcontext g_cudaContext = 0;

static bool execute()
{
    // Create the CameraProvider object
    UniqueObj<CameraProvider> cameraProvider(CameraProvider::create());
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(cameraProvider);
    if (!iCameraProvider)
        ORIGINATE_ERROR("Failed to create CameraProvider");

    // Get the camera devices.
    std::vector<CameraDevice*> cameraDevices;
    iCameraProvider->getCameraDevices(&cameraDevices);
    if (cameraDevices.size() == 0)
        ORIGINATE_ERROR("No cameras available");

    // Create the capture session using the first device.
    UniqueObj<CaptureSession> captureSession(
        iCameraProvider->createCaptureSession(cameraDevices[0]));
    ICaptureSession *iCaptureSession = interface_cast<ICaptureSession>(captureSession);
    if (!iCaptureSession)
        ORIGINATE_ERROR("Failed to create CaptureSession");

    printf("Creating output stream\n");
    UniqueObj<OutputStreamSettings> streamSettings(iCaptureSession->createOutputStreamSettings());
    IOutputStreamSettings *iStreamSettings = interface_cast<IOutputStreamSettings>(streamSettings);
    if (iStreamSettings)
    {
        iStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
        iStreamSettings->setResolution(STREAM_SIZE);
    }
    UniqueObj<OutputStream> outputStream(iCaptureSession->createOutputStream(streamSettings.get()));
    IStream *iStream = interface_cast<IStream>(outputStream);
    if (!iStream)
        ORIGINATE_ERROR("Failed to create OutputStream");

    // Initialize and connect CUDA as the EGLStream consumer.
    PROPAGATE_ERROR(initCUDA(&g_cudaContext));
    CUresult cuResult;
    CUeglStreamConnection cudaConnection;
    printf("Connecting CUDA to OutputStream as an EGLStream consumer\n");
    cuResult = cuEGLStreamConsumerConnect(&cudaConnection, iStream->getEGLStream());
    if (cuResult != CUDA_SUCCESS)
    {
        ORIGINATE_ERROR("Unable to connect CUDA to EGLStream as a consumer (CUresult %s)",
            getCudaErrorString(cuResult));
    }

    // Create capture request and enable output stream.
    UniqueObj<Request> request(iCaptureSession->createRequest());
    IRequest *iRequest = interface_cast<IRequest>(request);
    if (!iRequest)
        ORIGINATE_ERROR("Failed to create Request");
    iRequest->enableOutputStream(outputStream.get());

    // Submit some captures and calculate the histogram with CUDA
    UniquePointer<unsigned int> histogramData(new unsigned int[HISTOGRAM_BINS]);
    if (!histogramData)
        ORIGINATE_ERROR("Failed to allocate histogram");
    for (unsigned int frame = 0; frame < FRAME_COUNT; ++frame)
    {
        /*
         * For simplicity this example submits a capture then waits for an output.
         * This pattern will not provide the best possible performance as the camera
         * stack runs in a pipeline, it is best to keep submitting as many captures as
         * possible prior to waiting for the result.
         */
        printf("Submitting a capture request\n");
        {
            Argus::Status status;
            const uint64_t ONE_SECOND = 1000000000;
            uint32_t result = iCaptureSession->capture(request.get(), ONE_SECOND, &status);
            if (result == 0)
                ORIGINATE_ERROR("Failed to submit capture request (status %x)", status);
        }

        printf("Acquiring an image from the EGLStream\n");
        CUgraphicsResource cudaResource = 0;
        CUstream cudaStream = 0;
        cuResult = cuEGLStreamConsumerAcquireFrame(&cudaConnection, &cudaResource, &cudaStream, -1);
        if (cuResult != CUDA_SUCCESS)
        {
            ORIGINATE_ERROR("Unable to acquire an image frame from the EGLStream with CUDA as a "
                "consumer (CUresult %s).", getCudaErrorString(cuResult));
        }

        // Get the CUDA EGL frame.
        CUeglFrame cudaEGLFrame;
        cuResult = cuGraphicsResourceGetMappedEglFrame(&cudaEGLFrame, cudaResource, 0, 0);
        if (cuResult != CUDA_SUCCESS)
        {
            ORIGINATE_ERROR("Unable to get the CUDA EGL frame (CUresult %s).",
                getCudaErrorString(cuResult));
        }

        // Print the information contained in the CUDA EGL frame structure.
        PROPAGATE_ERROR(printCUDAEGLFrame(cudaEGLFrame));

        if ((cudaEGLFrame.eglColorFormat != CU_EGL_COLOR_FORMAT_YUV420_PLANAR) &&
            (cudaEGLFrame.eglColorFormat != CU_EGL_COLOR_FORMAT_YUV420_SEMIPLANAR) &&
            (cudaEGLFrame.eglColorFormat != CU_EGL_COLOR_FORMAT_YUV422_PLANAR) &&
            (cudaEGLFrame.eglColorFormat != CU_EGL_COLOR_FORMAT_YUV422_SEMIPLANAR))
        {
            ORIGINATE_ERROR("Only YUV color formats are supported");
        }
        if (cudaEGLFrame.cuFormat != CU_AD_FORMAT_UNSIGNED_INT8)
            ORIGINATE_ERROR("Only 8-bit unsigned int formats are supported");

        // Create a surface from the luminance plane
        CUDA_RESOURCE_DESC cudaResourceDesc;
        memset(&cudaResourceDesc, 0, sizeof(cudaResourceDesc));
        cudaResourceDesc.resType = CU_RESOURCE_TYPE_ARRAY;
        cudaResourceDesc.res.array.hArray = cudaEGLFrame.frame.pArray[0];
        CUsurfObject cudaSurfObj = 0;
        cuResult = cuSurfObjectCreate(&cudaSurfObj, &cudaResourceDesc);
        if (cuResult != CUDA_SUCCESS)
        {
            ORIGINATE_ERROR("Unable to create the surface object (CUresult %s)",
                getCudaErrorString(cuResult));
        }

        printf("Calculating histogram with %d bins...\n", HISTOGRAM_BINS);
        float time = histogram(cudaSurfObj, cudaEGLFrame.width, cudaEGLFrame.height,
            histogramData.get());
        printf("Finished after %f ms.\n", time);

        printf("Result:");
        for (unsigned int index = 0; index < HISTOGRAM_BINS; ++index)
        {
            if (index % 8 == 0)
                printf("\n%2d:", index);
            printf(" %8d", histogramData.get()[index]);
        }
        printf("\n");

        cuResult = cuSurfObjectDestroy(cudaSurfObj);
        if (cuResult != CUDA_SUCCESS)
        {
            ORIGINATE_ERROR("Unable to destroy the surface object (CUresult %s)",
                getCudaErrorString(cuResult));
        }

        cuResult = cuEGLStreamConsumerReleaseFrame(&cudaConnection, cudaResource, &cudaStream);
        if (cuResult != CUDA_SUCCESS)
        {
            ORIGINATE_ERROR("Unable to release the last frame acquired from the EGLStream "
                "(CUresult %s).", getCudaErrorString(cuResult));
        }
    }

    printf("Cleaning up\n");

    // Disconnect the Argus producer from the stream.
    /// @todo: This is a WAR for a bug in cuEGLStreamConsumerDisconnect (see bug 200239336).
    outputStream.reset();

    cuResult = cuEGLStreamConsumerDisconnect(&cudaConnection);
    if (cuResult != CUDA_SUCCESS)
    {
        ORIGINATE_ERROR("Unable to disconnect CUDA as a consumer from EGLStream (CUresult %s)",
            getCudaErrorString(cuResult));
    }

    PROPAGATE_ERROR(cleanupCUDA(&g_cudaContext));

    printf("Done\n");

    return true;
}

}; // namespace ArgusSamples

int main(int argc, const char *argv[])
{
    if (!ArgusSamples::execute())
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
