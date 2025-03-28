/******************************************************************************\
Copyright (c) 2005-2019, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#pragma once

#ifdef _WIN32

#include <d3d9.h>
#include <dxva2api.h>
#include <wrl/client.h>

#include <vector>

#include "base_allocator.h"

namespace nx::media::quick_sync::windows {

enum eTypeHandle
{
    DXVA2_PROCESSOR     = 0x00,
    DXVA2_DECODER       = 0x01
};

class D3DFrameAllocator: public BaseFrameAllocator
{
public:
    D3DFrameAllocator(IDirect3DDeviceManager9* pManager);
    virtual ~D3DFrameAllocator();
    virtual mfxStatus Close();
    virtual IDirect3DDeviceManager9* GetDeviceManager() const { return m_manager.Get(); };

    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle);

protected:
    virtual mfxStatus CheckRequestType(mfxFrameAllocRequest *request);
    virtual mfxStatus ReleaseResponse(mfxFrameAllocResponse *response);
    virtual mfxStatus AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    virtual mfxStatus ReallocImpl(mfxMemId midIn, const mfxFrameInfo *info, mfxU16 memType, mfxMemId *midOut);

    void DeallocateMids(mfxHDLPair** pairs, int n);

    std::vector<mfxHDLPair**> m_midsAllocated;

    Microsoft::WRL::ComPtr<IDirect3DDeviceManager9> m_manager;
    Microsoft::WRL::ComPtr<IDirectXVideoDecoderService> m_decoderService;
    Microsoft::WRL::ComPtr<IDirectXVideoProcessorService> m_processorService;
    HANDLE m_hDecoder;
    HANDLE m_hProcessor;
    DWORD m_surfaceUsage;
};

} // namespace nx::media::quick_sync::windows

#endif // #ifdef _WIN32
