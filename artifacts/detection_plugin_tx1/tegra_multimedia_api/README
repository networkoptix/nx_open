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

Build and Run Guide
===================

Build and run the samples by following the procedures in this document:

1. Export environment variables.
2. Install the NVIDIA(r) CUDA(r)/Opencv4tegra(r)/cuDNN(r)/
   GIE(r)(also known as TensorRT(tm)) via Jetpack.
3. Create symbolic links.
4. Build and run the samples.



To export environment variables
===============================

1. Export the ARM application binary interface based on the target
   platform with the following command:

        $ export TEGRA_ARMABI=aarch64-linux-gnu

2. Export the XDisplay with the following command:

        $ export DISPLAY=:0

To install the NVIDIA(r) CUDA(r)/Opencv4tegra(r)/cuDNN(r)/
GIE(r)(also known as TensorRT(tm)) via Jetpack
=========================================================

NOTE: If you have installed them, you can skip the following steps.

1. Download Jetpack from the following website:

        https://developer.nvidia.com/embedded/downloads

2. Run the installation script from the host machine with the
   following commmands:

        $ chmod +x  ./JetPack-L4T-<version>-linux-x64.run
        $ ./JetPack-L4T-<version>-linux-x64.run

3. Select "Jetson TX1 Development Kit(64bit) and Ubuntu host".

4. Select "custom" and click "clear action".

5. Select "CUDA Toolkit for L4T", "OpenCV for Tegra","cuDNN Package" and
   "TensorRT", and then install.

6. For installation details, see the _installer folder.

To create the needed symbolic links
===================================

*  Create symbolic links with the following commands:

    $ cd /usr/lib/${TEGRA_ARMABI}
    $ sudo ln -sf tegra-egl/libEGL.so.1 libEGL.so
    $ sudo ln -sf tegra-egl/libGLESv2.so.2 libGLESv2.so
    $ sudo ln -sf libv4l2.so.0 libv4l2.so

To build and run the samples
============================

1. Change directory to

        $HOME/tegra_multimedia_api/samples

   for the procedures in this section.

2. See the following file for details regarding cross-compilation (optional):

        tegra_multimedia_api/CROSS_PLATFORM_SUPPORT

Video Decode
------------

*  This sample demonstrates how to decode H.264/H.265 video from a local file
   with supported decoding features.
   Build and run 00_video_decode with the following commands:

        $ cd 00_video_decode
        $ make
        $ ./video_decode <in-file> <in-format> [options]

Encoding YUV to H.264/H.265
---------------------------

*  This sample demonstrates how to encode H.264/H.265 video from a local
   YUV file with supported encoding features.
   Build and run 01_video_encode with the following commands:

        $ cd 01_video_encode
        $ make
        $ ./video_encode <in-file> <in-width> <in-height> <encoder-type> <out-file> [OPTIONS]

Video Decode With CUDA Processing
---------------------------------

*  This sample demonstrates how to decode H.264/H.265 video from a local
   file and then share the YUV buffer with CUDA to draw a black box
   in the left corner.
   Build and run 02_video_dec_cuda with the following commands:

        $ cd 02_video_dec_cuda
        $ make
        $ ./video_dec_cuda <in-file> <in-format> [options]

CUDA Processing and Encoding YUV to H.264/H.265
-----------------------------------------------

*  This sample demonstrates how to use CUDA to draw a black box in the YUV
   buffer and then feed it to video encoder to generate an H.264/H.265 video file.
   Build and run 03_video_cuda_enc with the following commands:

        $ cd 03_video_cuda_enc
        $ make
        $ ./video_cuda_enc <in-file> <in-width> <in-height> <encoder-type> <out-file> [OPTIONS]

    Note: This sample can only accept I420 YUV streams.

Video Decode with GIE
---------------------

*  This sample demonstrates the simplest way to use GIE
   and save the bounding box info to the file result.txt.
   The pipeline:
     Input file -> DEC -> VIC -> GIE Inference -> BBox file
   Build and run 04_video_dec_gie with the following commands:

        $ cd 04_video_dec_gie
        $ make
        $ ./video_dec_gie <in-file> <in-format> [options]

   Notes:
   1. The result saves the normalized rectangle within [0,1].
   2. 02_video_dec_cuda can verify the result and scale the rectangle parameters
      with the following command:
        $ ./video_dec_cuda <in-file> <in-format> --bbox-file result.txt
   3. Supports in-stream resolution changes.
   4. The default deploy file is
        ../../data/model/GoogleNet-modified.prototxt
      and model file is
        ../../data/model/GoogleNet-modified-online_iter_30000.caffemodel
   5. The batch size can be changed in GoogleNet-modified.prototxt, but is limited
      by the available system memory. For the default network, the maximal safe value
      is 32 with the fp16 mode, or 24 with the fp32 mode.
   6. The following command must be executed before running with a new batch size:
        $ rm gieModel.cache

JPEG Encode
-----------

*  This sample demonstrates how to use libjpeg-8b APIs to encode JPEG
   image from software-allocated buffers.
   Build and run 05_jpeg_encode with the following commands:

        $ cd 05_jpeg_encode
        $ make
        $ ./jpeg_encode <in-file> <in-width> <in-height> <out-file> [OPTIONS]

JPEG Decode
-----------

*  This sample demonstrates how to use libjpeg-8b APIs to decode JPEG
   image from software-allocated buffers.
   Build and run 06_jpeg_decode with the following commands:

        $ cd 06_jpeg_decode
        $ make
        $ ./jpeg_decode <in-file> <out-file> [OPTIONS]

Video Format Conversion
-----------------------

*  This sample demonstrates how to use V4L2 APIs to do video format conversion and
   video scaling.
   Build and run 07_video_convert with the following commands:

        $ cd 07_video_convert
        $ make
        $ ./video_convert <in-file> <in-width> <in-height> <in-format> <out-file> <out-width> <out-height> <out-format> [OPTIONS]

JPEG Encoding of a Camera Preview Stream
----------------------------------------

*  This sample demonstrates how to use Argus API to preview camera
   stream and use libjpeg-8b APIs to encode JPEG image at the same
   time.
   Build and run 09_camera_jpeg_capture with the following commands:

        $ cd 09_camera_jpeg_capture
        $ make
        $ ./camera_jpeg_capture [OPTIONS]

   The captured preview stream can display over HDMI. JPEG files are
   stored in the current directory.

H.264 Encode with Camera Recording Stream
-----------------------------------------

*  This sample demonstrates how to use the Argus API to get the
   real-time camera stream and feed it into the video encoder to
   generate H.264/H.265 video files.
   Build and run 10_camera_recording with the following commands:

        $ cd 10_camera_recording
        $ make
        $ ./camera_recording [OPTIONS]

   The H.264 video file is created in the current directory.

Camera Object Identification
----------------------------

*  This sample demonstrates how to use Argus API to get the real-time
   camera stream and feed it to Caffe for object classification.

   For details regarding this sample, see 11_camera_object_identification/README.

Backend
-------

*  This sample demonstrates how to decode multi-channel H.264/H.265
   from a local file and then feed one of YUV channels into GIE for
   real-time inference. Finally, display the original image with BBOX
   from GIE.

   Backend with GIE(default)

        Ensure the following variable is set to 1 by making the
        following change in the Makefile:

        ENABLEGIE := 1

        Build and run backend with GIE with the following commands:

            $ cd backend
            $ make
            $ ./backend 1 ../../data/video/sample_outdoor_car_1080p_10fps.h264 H264 \
                --gie-deployfile ../../data/model/GoogleNet-modified.prototxt \
                --gie-modelfile ../../data/model/GoogleNet-modified-online_iter_30000.caffemodel \
                --gie-forcefp32 0 --gie-proc-interval 1 -fps 10

        Note: The GIE batch size can be configured from the third line of the following file

                  ../../data/model/GoogleNet-modified.prototxt

        Where the valid values are 1(default), or 2, or 4.

   Backend without GIE

        Ensure the following variable is set to 0 by making the
        following change in the Makefile:

        ENABLEGIE := 0

        Build and run backend without GIE with the following commands:

            $ cd backend
            $ make
            $ ./backend <channel-num> <in-file1> <in-file2>... <in-format> [options]
