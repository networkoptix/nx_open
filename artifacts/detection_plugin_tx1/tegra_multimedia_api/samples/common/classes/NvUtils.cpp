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

#include "NvUtils.h"
#include "NvBuffer.h"
#include "NvLogging.h"
#include <fstream>

int
read_video_frame(std::ifstream * stream, NvBuffer & buffer)
{
    uint32_t i, j;
    char *data;

    for (i = 0; i < buffer.n_planes; i++)
    {
        NvBuffer::NvBufferPlane &plane = buffer.planes[i];
        std::streamsize bytes_to_read =
            plane.fmt.bytesperpixel * plane.fmt.width;
        data = (char *) plane.data;
        plane.bytesused = 0;
        for (j = 0; j < plane.fmt.height; j++)
        {
            stream->read(data, bytes_to_read);
            if (stream->gcount() < bytes_to_read)
                return -1;
            data += plane.fmt.stride;
        }
        plane.bytesused = plane.fmt.stride * plane.fmt.height;
    }
    return 0;
}

int
write_video_frame(std::ofstream * stream, NvBuffer &buffer)
{
    uint32_t i, j;
    char *data;

    for (i = 0; i < buffer.n_planes; i++)
    {
        NvBuffer::NvBufferPlane &plane = buffer.planes[i];
        size_t bytes_to_write =
            plane.fmt.bytesperpixel * plane.fmt.width;

        data = (char *) plane.data;
        for (j = 0; j < plane.fmt.height; j++)
        {
            stream->write(data, bytes_to_write);
            if (!stream->good())
                return -1;
            data += plane.fmt.stride;
        }
    }
    return 0;
}
