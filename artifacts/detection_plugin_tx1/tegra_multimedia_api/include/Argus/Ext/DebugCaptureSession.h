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

#ifndef _ARGUS_DEBUG_CAPTURE_SESSION_H
#define _ARGUS_DEBUG_CAPTURE_SESSION_H

namespace Argus
{

/**
 * The Ext::DebugCaptureSession extension adds internal method to
 * the Argus driver. It introduces one new interface:
 *   - IDebugCaptureSession: used to dump session runtime information
 */

DEFINE_UUID(ExtensionName, EXT_DEBUG_CAPTURE_SESSION,
                                             1fee5f03,2ea9,4558,8e92,c2,4b,0b,82,b9,af);

namespace Ext
{

/**
 * @class IDebugCaptureSession
 *
 * Interface used to dump session runtime information
 *
 * This interface is available from the CaptureSession class.
 */

DEFINE_UUID(InterfaceID, IID_DEBUG_CAPTURE_SESSION, beaa075b,dcf7,4e26,b255,3c,98,db,03,5b,99);

class IDebugCaptureSession : public Interface
{
public:
    static const InterfaceID& id() { return IID_DEBUG_CAPTURE_SESSION; }

    /**
     * Returns session runtime information to the specified file descriptor.
     */
    virtual Status dump(int32_t fd) const = 0;

protected:
    ~IDebugCaptureSession() {}
};

} // namespace Ext

} // namespace Argus

#endif

