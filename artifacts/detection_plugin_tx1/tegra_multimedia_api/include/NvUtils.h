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

/**
 * @file NvUtils.h
 *
 * @brief Common functions used by applications
 */

#ifndef __NV_UTILS_H_
#define __NV_UTILS_H_

#include <fstream>
#include "NvBuffer.h"

/**
 * Read a video frame from a file to the Buffer structure
 *
 * This function reads data from the file into the buffer plane by plane.
 * It reads width * height * byteperpixel of data for each plane while taking
 * care of the stride of the plane.
 *
 * @param[in] stream Input file stream
 * @param[in] buffer Buffer object into which the data has to be read
 * @returns 0 for success, -1 for failure
 */
int read_video_frame(std::ifstream * stream, NvBuffer & buffer);

/**
 * Wite a video frame to a file from the Buffer structure
 *
 * This function writes data to the file from the buffer plane by plane.
 * It writes width * height * byteperpixel of data for each plane while taking
 * care of the stride of the plane.
 *
 * @param[in] stream Output file stream
 * @param[in] buffer Buffer object from which the data has to be written
 * @returns 0 for success, -1 for failure
 */
int write_video_frame(std::ofstream * stream, NvBuffer & buffer);
#endif
