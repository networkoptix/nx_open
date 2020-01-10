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

#include <stdlib.h>

#include "mfx_buffering.h"

CBuffering::CBuffering():
    m_SurfacesNumber(0),
    m_pSurfaces(NULL),
    m_FreeSurfacesPool(m_Mutex),
    m_UsedSurfacesPool(m_Mutex)
{
}

CBuffering::~CBuffering()
{
}

mfxStatus
CBuffering::AllocBuffers(mfxU32 SurfaceNumber)
{
    if (!SurfaceNumber)
        return MFX_ERR_MEMORY_ALLOC;

    m_SurfacesNumber = SurfaceNumber;
    m_pSurfaces = (msdkFrameSurface*)calloc(m_SurfacesNumber, sizeof(msdkFrameSurface));
    if (!m_pSurfaces) return MFX_ERR_MEMORY_ALLOC;

    ResetBuffers();
    return MFX_ERR_NONE;
}

void
CBuffering::FreeBuffers()
{
    if (m_pSurfaces) {
        free(m_pSurfaces);
        m_pSurfaces = NULL;
    }

    m_UsedSurfacesPool.m_pSurfacesHead = NULL;
    m_UsedSurfacesPool.m_pSurfacesTail = NULL;
    m_FreeSurfacesPool.m_pSurfaces = NULL;
}

void
CBuffering::ResetBuffers()
{
    mfxU32 i;
    msdkFrameSurface* pFreeSurf = m_FreeSurfacesPool.m_pSurfaces = m_pSurfaces;

    for (i = 0; i < m_SurfacesNumber; ++i) {
        if (i < (m_SurfacesNumber-1)) {
            pFreeSurf[i].next = &(pFreeSurf[i+1]);
            pFreeSurf[i+1].prev = &(pFreeSurf[i]);
        }
    }
}

void
CBuffering::SyncFrameSurfaces()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    msdkFrameSurface *next;
    next = NULL;
    msdkFrameSurface *cur = m_UsedSurfacesPool.m_pSurfacesHead;

    while (cur) {
        if (cur->frame.Data.Locked || cur->render_lock) {
            // frame is still locked: just moving to the next one
            cur = cur->next;
        } else {
            // frame was unlocked: moving it to the free surfaces array
            m_UsedSurfacesPool.DetachSurfaceUnsafe(cur);
            m_FreeSurfacesPool.AddSurfaceUnsafe(cur);

            cur = next;
        }
    }
}
