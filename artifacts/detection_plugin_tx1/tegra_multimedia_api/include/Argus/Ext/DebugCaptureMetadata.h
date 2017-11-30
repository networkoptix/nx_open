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

#ifndef _ARGUS_DEBUG_CAPTURE_METADATA_H
#define _ARGUS_DEBUG_CAPTURE_METADATA_H

namespace Argus
{

/**
 * The Ext::DebugCaptureMetadata extension adds internal capture metadata to
 * the Argus driver. It introduces one new interface:
 *   - IDebugCaptureMetadata: used to get hardware capture count for a Request.
 */

DEFINE_UUID(ExtensionName, EXT_DEBUG_CAPTURE_METADATA, 37afdbd9,0020,4f91,957b,46,ea,eb,79,80,c7);
namespace Ext
{

/**
 * @class IDebugCaptureMetadata
 *
 * Interface used to query hardware capture count for a request.
 *
 * This interface is available from the CaptureMetadata class.
 */

DEFINE_UUID(InterfaceID, IID_DEBUG_CAPTURE_METADATA,
                             c21a7ba1,2b3f,4275,8469,a2,56,34,93,53,93);

class IDebugCaptureMetadata : public Interface
{
public:
    static const InterfaceID& id() { return IID_DEBUG_CAPTURE_METADATA; }

    /**
     * Returns hardware capture count for a request.
     * Hardware capture counter begin counting after sensor opened.
     */
    virtual uint32_t getHwFrameCount() const = 0;

protected:
    ~IDebugCaptureMetadata() {}
};

} // namespace Ext

} // namespace Argus

#endif

