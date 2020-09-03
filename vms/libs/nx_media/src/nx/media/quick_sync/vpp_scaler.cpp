#include "vpp_scaler.h"

#include <string.h>
#include <thread>

#include <nx/utils/log/log.h>

#define ALIGN16(value) (((value + 15) >> 4) << 4) // round up to a multiple of 16
#define ALIGN32(value) (((value + 31) >> 5) << 5) // round up to a multiple of 32

// Usage of the following two macros are only required for certain Windows DirectX11 use cases
#define WILL_READ  0x1000
#define WILL_WRITE 0x2000

int GetFreeSurfaceIndex(const std::vector<mfxFrameSurface1>& pSurfacesPool)
{
    auto it = std::find_if(pSurfacesPool.begin(), pSurfacesPool.end(), [](const mfxFrameSurface1& surface) {
        return 0 == surface.Data.Locked;
    });

    if (it == pSurfacesPool.end())
        return MFX_ERR_NOT_FOUND;
    else return it - pSurfacesPool.begin();
}

namespace nx::media::quick_sync {
VppScaler::VppScaler(MFXVideoSession& session, std::shared_ptr<MFXFrameAllocator> allocator):
    m_mfxSession(session),
    m_allocator(allocator)
{
}

VppScaler::~VppScaler()
{
    close();
}

void VppScaler::close()
{
    if (m_vpp)
    {
        m_allocator->Free(m_allocator->pthis, &m_mfxResponseOut);
        m_outputSurfaces.clear();
        m_vpp->Close();
        m_vpp.reset();
    }
}

mfxVideoParam VppScaler::buildParam(const QSize& sourceSize, const QSize& targetSize)
{
    mfxVideoParam params;
    memset(&params, 0, sizeof(params));
    // Input data
    params.vpp.In.FourCC = MFX_FOURCC_NV12;
    params.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    params.vpp.In.CropX = 0;
    params.vpp.In.CropY = 0;
    params.vpp.In.CropW = sourceSize.width();
    params.vpp.In.CropH = sourceSize.height();
    params.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    params.vpp.In.FrameRateExtN = 30;
    params.vpp.In.FrameRateExtD = 1;
    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    params.vpp.In.Width = ALIGN16(sourceSize.width());
    params.vpp.In.Height =
        (MFX_PICSTRUCT_PROGRESSIVE == params.vpp.In.PicStruct)
        ? ALIGN16(sourceSize.height())
        : ALIGN32(sourceSize.height());
    // Output data
    params.vpp.Out.FourCC = MFX_FOURCC_NV12;
    params.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    params.vpp.Out.CropX = 0;
    params.vpp.Out.CropY = 0;
    params.vpp.Out.CropW = targetSize.width();
    params.vpp.Out.CropH = targetSize.height();
    params.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    params.vpp.Out.FrameRateExtN = 30;
    params.vpp.Out.FrameRateExtD = 1;
    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    params.vpp.Out.Width = ALIGN16(params.vpp.Out.CropW);
    params.vpp.Out.Height =
        (MFX_PICSTRUCT_PROGRESSIVE == params.vpp.Out.PicStruct)
        ? ALIGN16(params.vpp.Out.CropH)
        : ALIGN32(params.vpp.Out.CropH);

    params.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    return params;
}

bool VppScaler::allocSurfaces(const mfxVideoParam& params)
{
    if (!m_outputSurfaces.empty())
    {
        m_allocator->Free(m_allocator->pthis, &m_mfxResponseOut);
        m_outputSurfaces.clear();
    }
    // Allocate required surfaces
    m_VPPRequest[1].NumFrameMin = 1;
    m_VPPRequest[1].NumFrameSuggested = 1;
    mfxStatus status = m_allocator->Alloc(m_allocator->pthis, &m_VPPRequest[1], &m_mfxResponseOut);
    if (status < MFX_ERR_NONE)
    {
        NX_WARNING(this, "Failed to alloc VPP surfaces, error: %1", status);
        return false;
    }

    m_outputSurfaces.resize(m_mfxResponseOut.NumFrameActual);
    for (int i = 0; i < m_mfxResponseOut.NumFrameActual; i++)
    {
        memset(&m_outputSurfaces[i], 0, sizeof(mfxFrameSurface1));
        m_outputSurfaces[i].Info = params.vpp.Out;
        m_outputSurfaces[i].Data.MemId = m_mfxResponseOut.mids[i];    // MID (memory id) represent one D3D NV12 surface
    }
    return true;
}

bool VppScaler::init(const QSize& sourceSize, const QSize& targetSize)
{
    mfxVideoParam params = buildParam(sourceSize, targetSize);
    mfxStatus status;
    if (m_vpp)
    {
        // VPP already initialized, reset parameters
        status = m_vpp->Reset(&params);
        if (status < MFX_ERR_NONE)
        {
            NX_WARNING(this, "Failed to reset VPP, error: %1", status);
            return false;
        }
        return allocSurfaces(params);
    }

    m_vpp = std::make_unique<MFXVideoVPP>(m_mfxSession);

    // Query number of required surfaces for VPP
    memset(&m_VPPRequest, 0, sizeof(mfxFrameAllocRequest) * 2);
    status = m_vpp->QueryIOSurf(&params, m_VPPRequest);
    if (status < MFX_ERR_NONE)
    {
        NX_WARNING(this, "Failed to query VPP params, error: %1", status);
        return false;
    }

    m_VPPRequest[0].Type |= WILL_WRITE; // This line is only required for Windows DirectX11 to ensure that surfaces can be written to by the application
    m_VPPRequest[1].Type |= WILL_READ; // This line is only required for Windows DirectX11 to ensure that surfaces can be retrieved by the application

    if (!allocSurfaces(params))
        return false;

    status = m_vpp->Init(&params);
    if (status == MFX_WRN_PARTIAL_ACCELERATION)
        status = MFX_ERR_NONE;

    if (status < MFX_ERR_NONE)
    {
        NX_WARNING(this, "Failed to init VPP session, error: %1", status);
        return false;
    }
    m_targetSize = targetSize;
    m_sourceSize = sourceSize;
    return true;
}

bool VppScaler::scaleFrame(
    mfxFrameSurface1* inputSurface, mfxFrameSurface1** outSurface, const QSize& targetSize)
{
    QSize sourceSize(inputSurface->Info.Width, inputSurface->Info.Height);
    if (sourceSize != m_sourceSize || targetSize != m_targetSize)
    {
        if (!init(sourceSize, targetSize))
        {
            NX_WARNING(this, "Failed to init VPP scaler source size %1, target size %2",
                sourceSize, targetSize);
            return false;
        }
    }

    if (!m_vpp)
    {
        *outSurface = inputSurface;
        return true;
    }

    *outSurface = nullptr;
    int status = MFX_ERR_NONE;
    auto surfaceIndex = GetFreeSurfaceIndex(m_outputSurfaces); // Find free output frame surface
    if (surfaceIndex == MFX_ERR_NOT_FOUND)
    {
        NX_WARNING(this, "No free surfaces");
        return false;
    }

    mfxSyncPoint syncp;
    for (;;)
    {
        status = m_vpp->RunFrameVPPAsync(inputSurface, &m_outputSurfaces[surfaceIndex], nullptr, &syncp);
        if (MFX_WRN_DEVICE_BUSY == status)
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Wait if device is busy, then repeat the same call
        else
            break;
    }

    if (MFX_ERR_NONE != status)
        return false;

    status = m_mfxSession.SyncOperation(syncp, 60000); // Synchronize. Wait until frame processing is ready
    if (status < MFX_ERR_NONE)
    {
        NX_WARNING(this, "Failed to sync VPP result, error: %1", status);
        return false;
    }

    *outSurface = &m_outputSurfaces[surfaceIndex];
    return true;
}


} // namespace nx::media::quick_sync
