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

#include "sysmem_allocator.h"

#define ID_BUFFER MFX_MAKEFOURCC('B','U','F','F')
#define ID_FRAME  MFX_MAKEFOURCC('F','R','M','E')

mfxStatus AllocBuffer(mfxU32 nbytes, mfxU16 type, mfxMemId *mid);
mfxStatus LockBuffer(mfxMemId mid, mfxU8 **ptr);
mfxStatus UnlockBuffer(mfxMemId mid);
mfxStatus FreeBuffer(mfxMemId mid);

SysMemFrameAllocator::~SysMemFrameAllocator()
{
    Close();
}

mfxStatus SysMemFrameAllocator::Close()
{
    return BaseFrameAllocator::Close();
}

mfxStatus SysMemFrameAllocator::LockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    if (!ptr)
        return MFX_ERR_NULL_PTR;

    // If allocator uses pointers instead of mids, no further action is required
    if (!mid && ptr->Y)
        return MFX_ERR_NONE;

    sFrame *fs = 0;
    mfxStatus sts = LockBuffer(mid,(mfxU8 **)&fs);

    if (MFX_ERR_NONE != sts)
        return sts;

    if (ID_FRAME != fs->id)
    {
        UnlockBuffer(mid);
        return MFX_ERR_INVALID_HANDLE;
    }

    mfxU16 Width2 = (mfxU16)MSDK_ALIGN32(fs->info.Width);
    mfxU16 Height2 = (mfxU16)MSDK_ALIGN32(fs->info.Height);
    ptr->B = ptr->Y = (mfxU8 *)fs + MSDK_ALIGN32(sizeof(sFrame));

    switch (fs->info.FourCC)
    {
    case MFX_FOURCC_NV12:
        ptr->U = ptr->Y + Width2 * Height2;
        ptr->V = ptr->U + 1;
        ptr->PitchHigh = 0;
        ptr->PitchLow = (mfxU16)MSDK_ALIGN32(fs->info.Width);
        break;
    case MFX_FOURCC_NV16:
        ptr->U = ptr->Y + Width2 * Height2;
        ptr->V = ptr->U + 1;
        ptr->PitchHigh = 0;
        ptr->PitchLow = (mfxU16)MSDK_ALIGN32(fs->info.Width);
        break;
    case MFX_FOURCC_YV12:
        ptr->V = ptr->Y + Width2 * Height2;
        ptr->U = ptr->V + (Width2 >> 1) * (Height2 >> 1);
        ptr->PitchHigh = 0;
        ptr->PitchLow = (mfxU16)MSDK_ALIGN32(fs->info.Width);
        break;
    case MFX_FOURCC_UYVY:
        ptr->U = ptr->Y;
        ptr->Y = ptr->U + 1;
        ptr->V = ptr->U + 2;
        ptr->PitchHigh = (mfxU16)((2 * MSDK_ALIGN32(fs->info.Width)) / (1 << 16));
        ptr->PitchLow = (mfxU16)((2 * MSDK_ALIGN32(fs->info.Width)) % (1 << 16));
        break;
    case MFX_FOURCC_YUY2:
        ptr->U = ptr->Y + 1;
        ptr->V = ptr->Y + 3;
        ptr->PitchHigh = (mfxU16)((2 * MSDK_ALIGN32(fs->info.Width)) / (1 << 16));
        ptr->PitchLow = (mfxU16)((2 * MSDK_ALIGN32(fs->info.Width)) % (1 << 16));
        break;
#if (MFX_VERSION >= 1028)
    case MFX_FOURCC_RGB565:
        ptr->G = ptr->B;
        ptr->R = ptr->B;
        ptr->PitchHigh = (mfxU16)((2 * MSDK_ALIGN32(fs->info.Width)) / (1 << 16));
        ptr->PitchLow = (mfxU16)((2 * MSDK_ALIGN32(fs->info.Width)) % (1 << 16));
        break;
#endif
    case MFX_FOURCC_RGB3:
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->PitchHigh = (mfxU16)((3 * MSDK_ALIGN32(fs->info.Width)) / (1 << 16));
        ptr->PitchLow = (mfxU16)((3 * MSDK_ALIGN32(fs->info.Width)) % (1 << 16));
        break;
#if !(defined(_WIN32) || defined(_WIN64))
    case MFX_FOURCC_RGBP:
        ptr->G = ptr->B + Width2 * Height2;
        ptr->R = ptr->B + Width2 * Height2 * 2;
        ptr->PitchHigh = (mfxU16)((MSDK_ALIGN32(fs->info.Width)) / (1 << 16));
        ptr->PitchLow = (mfxU16)((MSDK_ALIGN32(fs->info.Width)) % (1 << 16));
        break;
#endif
    case MFX_FOURCC_RGB4:
    case MFX_FOURCC_A2RGB10:
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        ptr->PitchHigh = (mfxU16)((4 * MSDK_ALIGN32(fs->info.Width)) / (1 << 16));
        ptr->PitchLow = (mfxU16)((4 * MSDK_ALIGN32(fs->info.Width)) % (1 << 16));
        break;
    case MFX_FOURCC_R16:
        ptr->Y16 = (mfxU16 *)ptr->B;
        ptr->PitchHigh = (mfxU16)((2 * MSDK_ALIGN32(fs->info.Width)) / (1 << 16));
        ptr->PitchLow = (mfxU16)((2 * MSDK_ALIGN32(fs->info.Width)) % (1 << 16));
        break;
    case MFX_FOURCC_P010:
        ptr->U = ptr->Y + Width2 * Height2 * 2;
        ptr->V = ptr->U + 2;
        ptr->PitchHigh = 0;
        ptr->PitchLow = (mfxU16)MSDK_ALIGN32(fs->info.Width * 2);
        break;
    case MFX_FOURCC_P210:
        ptr->U = ptr->Y + Width2 * Height2 * 2;
        ptr->V = ptr->U + 2;
        ptr->PitchHigh = 0;
        ptr->PitchLow = (mfxU16)MSDK_ALIGN32(fs->info.Width * 2);
        break;
    case MFX_FOURCC_AYUV:
        ptr->V = ptr->B;
        ptr->U = ptr->V + 1;
        ptr->Y = ptr->V + 2;
        ptr->A = ptr->V + 3;
        ptr->PitchHigh = (mfxU16)((4 * MSDK_ALIGN32(fs->info.Width)) / (1 << 16));
        ptr->PitchLow = (mfxU16)((4 * MSDK_ALIGN32(fs->info.Width)) % (1 << 16));
        break;
#if (MFX_VERSION >= 1027)
    case MFX_FOURCC_Y210:
        ptr->Y16 = (mfxU16 *)ptr->B;
        ptr->U16 = ptr->Y16 + 1;
        ptr->V16 = ptr->Y16 + 3;
        //4 words per macropixel -> 2 words per pixel -> 4 bytes per pixel
        ptr->PitchHigh = (mfxU16)((4 * MSDK_ALIGN32(fs->info.Width)) / (1 << 16));
        ptr->PitchLow = (mfxU16)((4 * MSDK_ALIGN32(fs->info.Width)) % (1 << 16));
        break;
    case MFX_FOURCC_Y410:
        ptr->U = ptr->V = ptr->A = ptr->Y;
        ptr->PitchHigh = (mfxU16)((4 * MSDK_ALIGN32(fs->info.Width)) / (1 << 16));
        ptr->PitchLow = (mfxU16)((4 * MSDK_ALIGN32(fs->info.Width)) % (1 << 16));
        break;
#endif

    default:
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

mfxStatus SysMemFrameAllocator::UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    // If allocator uses pointers instead of mids, no further action is required
    if (!mid && ptr->Y)
        return MFX_ERR_NONE;

    mfxStatus sts = UnlockBuffer(mid);

    if (MFX_ERR_NONE != sts)
        return sts;

    if (NULL != ptr)
    {
        ptr->Pitch = 0;
        ptr->Y     = 0;
        ptr->U     = 0;
        ptr->V     = 0;
        ptr->A     = 0;
    }

    return MFX_ERR_NONE;
}

mfxStatus SysMemFrameAllocator::GetFrameHDL(mfxMemId /*mid*/, mfxHDL* /*handle*/)
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus SysMemFrameAllocator::CheckRequestType(mfxFrameAllocRequest *request)
{
    mfxStatus sts = BaseFrameAllocator::CheckRequestType(request);
    if (MFX_ERR_NONE != sts)
        return sts;

    if ((request->Type & MFX_MEMTYPE_SYSTEM_MEMORY) != 0)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_UNSUPPORTED;
}

mfxMemId *SysMemFrameAllocator::GetMidHolder(mfxMemId mid)
{
    for (auto resp : m_vResp)
    {
        mfxMemId *it = std::find(resp->mids, resp->mids + resp->NumFrameActual, mid);
        if (it != resp->mids + resp->NumFrameActual)
            return it;
    }
    return nullptr;
}

mfxU32 GetSurfaceSize(mfxU32 FourCC, mfxU32 Width2, mfxU32 Height2)
{
    mfxU32 nbytes = 0;

    switch (FourCC)
    {
    case MFX_FOURCC_YV12:
    case MFX_FOURCC_NV12:
        nbytes = Width2*Height2 + (Width2>>1)*(Height2>>1) + (Width2>>1)*(Height2>>1);
        break;
    case MFX_FOURCC_NV16:
        nbytes = Width2*Height2 + (Width2>>1)*(Height2) + (Width2>>1)*(Height2);
        break;
#if (MFX_VERSION >= 1028)
    case MFX_FOURCC_RGB565:
        nbytes = 2*Width2*Height2;
        break;
#endif
#if !(defined(_WIN32) || defined(_WIN64))
    case MFX_FOURCC_RGBP:
#endif
    case MFX_FOURCC_RGB3:
        nbytes = Width2*Height2 + Width2*Height2 + Width2*Height2;
        break;
    case MFX_FOURCC_RGB4:
    case MFX_FOURCC_AYUV:
#if (MFX_VERSION >= 1027)
    case MFX_FOURCC_Y410:
#endif
        nbytes = Width2*Height2 + Width2*Height2 + Width2*Height2 + Width2*Height2;
        break;
    case MFX_FOURCC_UYVY:
    case MFX_FOURCC_YUY2:
        nbytes = Width2*Height2 + (Width2>>1)*(Height2) + (Width2>>1)*(Height2);
        break;
    case MFX_FOURCC_R16:
        nbytes = 2*Width2*Height2;
        break;
    case MFX_FOURCC_P010:
        nbytes = Width2*Height2 + (Width2>>1)*(Height2>>1) + (Width2>>1)*(Height2>>1);
        nbytes *= 2;
        break;
    case MFX_FOURCC_A2RGB10:
        nbytes = Width2*Height2*4; // 4 bytes per pixel
        break;
    case MFX_FOURCC_P210:
#if (MFX_VERSION >= 1027)
    case MFX_FOURCC_Y210:
#endif
        nbytes = Width2*Height2 + (Width2>>1)*(Height2) + (Width2>>1)*(Height2);
        nbytes *= 2; // 16bits
        break;


    default:
        break;
    }

    return nbytes;
}

mfxStatus SysMemFrameAllocator::ReallocImpl(mfxMemId mid, const mfxFrameInfo *info, mfxU16 /*memType*/, mfxMemId *midOut)
{
    if (!info || !midOut)
        return MFX_ERR_NULL_PTR;

    mfxU32 nbytes = GetSurfaceSize(info->FourCC, MSDK_ALIGN32(info->Width), MSDK_ALIGN32(info->Height));
    if(!nbytes)
        return MFX_ERR_UNSUPPORTED;

    // pointer to the record in m_mids structure
    mfxMemId *pmid = GetMidHolder(mid);
    if (!pmid)
        return MFX_ERR_MEMORY_ALLOC;

    mfxStatus sts = FreeBuffer(*pmid);
    if (MFX_ERR_NONE != sts)
        return sts;

    sts = AllocBuffer(
        MSDK_ALIGN32(nbytes) + MSDK_ALIGN32(sizeof(sFrame)), MFX_MEMTYPE_SYSTEM_MEMORY, pmid);
    if (MFX_ERR_NONE != sts)
        return sts;

    sFrame *fs;
    sts = LockBuffer(*pmid, (mfxU8 **)&fs);
    if (MFX_ERR_NONE != sts)
        return sts;

    fs->id = ID_FRAME;
    fs->info = *info;
    UnlockBuffer(*pmid);

    *midOut = *pmid;
    return MFX_ERR_NONE;
}

mfxStatus SysMemFrameAllocator::AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    mfxU32 numAllocated = 0;

    mfxU32 nbytes = GetSurfaceSize(request->Info.FourCC, MSDK_ALIGN32(request->Info.Width), MSDK_ALIGN32(request->Info.Height));
    if(!nbytes)
        return MFX_ERR_UNSUPPORTED;

    std::unique_ptr<mfxMemId[]> mids(new mfxMemId[request->NumFrameSuggested]);

    // allocate frames
    for (numAllocated = 0; numAllocated < request->NumFrameSuggested; numAllocated ++)
    {
        mfxStatus sts = AllocBuffer(nbytes + MSDK_ALIGN32(sizeof(sFrame)), request->Type, &(mids[numAllocated]));

        if (MFX_ERR_NONE != sts)
            break;

        sFrame *fs;
        sts = LockBuffer(mids[numAllocated], (mfxU8 **)&fs);

        if (MFX_ERR_NONE != sts)
            break;

        fs->id = ID_FRAME;
        fs->info = request->Info;
        sts = UnlockBuffer(mids[numAllocated]);

        if (MFX_ERR_NONE != sts)
            break;
    }

    // check the number of allocated frames
    if (numAllocated < request->NumFrameSuggested)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    response->NumFrameActual = (mfxU16) numAllocated;
    response->mids = mids.release();

    m_vResp.insert(response);
    return MFX_ERR_NONE;
}

mfxStatus SysMemFrameAllocator::ReleaseResponse(mfxFrameAllocResponse *response)
{
    if (!response)
        return MFX_ERR_NULL_PTR;

    mfxStatus sts = MFX_ERR_NONE;

    if (response->mids)
    {
        for (mfxU32 i = 0; i < response->NumFrameActual; i++)
        {
            if (response->mids[i])
            {
                sts = FreeBuffer(response->mids[i]);
                if (MFX_ERR_NONE != sts)
                    return sts;
            }
        }
    }
    m_vResp.erase(response);
    delete [] response->mids;
    response->mids = 0;

    return sts;
}

mfxStatus AllocBuffer(mfxU32 nbytes, mfxU16 type, mfxMemId *mid)
{
    if (!mid)
        return MFX_ERR_NULL_PTR;

    if (0 == (type & MFX_MEMTYPE_SYSTEM_MEMORY))
        return MFX_ERR_UNSUPPORTED;

    mfxU32 header_size = MSDK_ALIGN32(sizeof(sBuffer));
    mfxU8 *buffer_ptr = (mfxU8 *)calloc(header_size + nbytes + 32, 1);

    if (!buffer_ptr)
        return MFX_ERR_MEMORY_ALLOC;

    sBuffer *bs = (sBuffer *)buffer_ptr;
    bs->id = ID_BUFFER;
    bs->type = type;
    bs->nbytes = nbytes;
    *mid = (mfxHDL) bs;
    return MFX_ERR_NONE;
}

mfxStatus LockBuffer(mfxMemId mid, mfxU8 **ptr)
{
    if (!ptr)
        return MFX_ERR_NULL_PTR;

    sBuffer *bs = (sBuffer *)mid;

    if (!bs)
        return MFX_ERR_INVALID_HANDLE;
    if (ID_BUFFER != bs->id)
        return MFX_ERR_INVALID_HANDLE;

    *ptr = (mfxU8*)((size_t)((mfxU8 *)bs+MSDK_ALIGN32(sizeof(sBuffer))+31)&(~((size_t)31)));
    return MFX_ERR_NONE;
}

mfxStatus UnlockBuffer(mfxMemId mid)
{
    sBuffer *bs = (sBuffer *)mid;

    if (!bs || ID_BUFFER != bs->id)
        return MFX_ERR_INVALID_HANDLE;

    return MFX_ERR_NONE;
}

mfxStatus FreeBuffer(mfxMemId mid)
{
    sBuffer *bs = (sBuffer *)mid;
    if (!bs || ID_BUFFER != bs->id)
        return MFX_ERR_INVALID_HANDLE;

    free(bs);
    return MFX_ERR_NONE;
}
