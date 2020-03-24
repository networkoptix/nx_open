
#include "quick_sync_video_decoder_impl.h"

#include <deque>
#include <thread>

#include <QtMultimedia/QVideoFrame>

#include <utils/media/nalUnits.h>
#include "allocators/sysmem_allocator.h"
#include "allocators/vaapi_allocator.h"
#include "mfx_sys_qt_video_buffer.h"
#include "mfx_drm_qt_video_buffer.h"
#include "mfx_status_string.h"
#include "va_display.h"
#include "glx/va_glx.h"

#define MSDK_ALIGN16(value) (((value + 15) >> 4) << 4) // round up to a multiple of 16

namespace nx::media {

constexpr int kLatencySurfaceCount = 1;
constexpr int kSyncWaitMsec = 5000;
constexpr int kMaxBitstreamSizeBytes = 1024 * 1024 * 10;

bool QuickSyncVideoDecoderImpl::isCompatible(AVCodecID codec)
{
    if (VaDisplay::getDisplay() == nullptr)
        return false;

    if (codec == AV_CODEC_ID_H264 || codec == AV_CODEC_ID_H265)
        return true;
    return false;
}

QuickSyncVideoDecoderImpl::QuickSyncVideoDecoderImpl()
{
    memset(&m_response, 0, sizeof(m_response));
}

QuickSyncVideoDecoderImpl::~QuickSyncVideoDecoderImpl()
{
    NX_DEBUG(this, "Close quick sync video decoder");
    if (m_allocator)
        m_allocator->FreeFrames(&m_response);

    if (m_renderingSurface)
        vaDestroySurfaceGLX(m_display, m_renderingSurface);

    m_mfxSession.Close();
}

bool QuickSyncVideoDecoderImpl::initSession()
{
    mfxIMPL impl = MFX_IMPL_AUTO_ANY;
    mfxVersion version = { {0, 1} };
    mfxStatus status = m_mfxSession.Init(impl, &version);
    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(this, "Failed to init MFX session, error: %1", status);
        return false;
    }
    NX_DEBUG(this, "MFX version %1.%2", version.Major, version.Minor);
    m_display = VaDisplay::getDisplay();
    if (!m_display)
    {
        NX_ERROR(this, "Failed to get VA display");
        return false;
    }

    status = m_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, m_display);
    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(this, "Failed to set VA handle to MFX session, error: %1", status);
        return false;
    }

    // create memory allocator
    if (m_config.useVideoMemory)
    {
        m_allocator = std::make_unique<VaapiFrameAllocator>();
        vaapiAllocatorParams vaapiAllocParams;
        vaapiAllocParams.m_dpy = m_display;
        status = m_allocator->Init(&vaapiAllocParams);
    }
    else
    {
        m_allocator = std::make_unique<SysMemFrameAllocator>();
        status = m_allocator->Init(nullptr);
    }

    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(this, "Failed to init allocator, error code: %1", status);
        return false;
    }
    m_mfxSession.SetFrameAllocator(m_allocator.get());
    return true;
}

bool QuickSyncVideoDecoderImpl::allocFrames()
{
    mfxStatus status = MFXVideoDECODE_Query(m_mfxSession, &m_mfxDecParams, &m_mfxDecParams);
    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(this, "Query failed, error: %1", status);
        return false;
    }

    mfxFrameAllocRequest request;
    memset(&request, 0, sizeof(request));
    status = MFXVideoDECODE_QueryIOSurf(m_mfxSession, &m_mfxDecParams, &request);
    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(this, "QueryIOSurf failed, error: %1", status);
        return false;
    }

    // Add surfaces for rendering smoothness
    constexpr int kMaxFps = 30;
    request.NumFrameSuggested += kMaxFps / 5;

    if (request.NumFrameSuggested < m_mfxDecParams.AsyncDepth)
    {
        NX_ERROR(this, "NumFrameSuggested(%1) less then AsyncDepth(%2)",
            request.NumFrameSuggested, m_mfxDecParams.AsyncDepth);
        return false;
    }
    if (m_config.useVideoMemory)
        request.Type |= MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET/* | MFX_MEMTYPE_EXPORT_FRAME*/;
    else
        request.Type |= MFX_MEMTYPE_SYSTEM_MEMORY;
    request.NumFrameMin = request.NumFrameSuggested;
    return allocFramesSample(request);
}

bool QuickSyncVideoDecoderImpl::reallocCurrentSurface(mfxFrameSurface1& surface)
{
    mfxVideoParam param {};
    mfxStatus sts = MFXVideoDECODE_GetVideoParam(m_mfxSession, &param);
    if (MFX_ERR_NONE != sts)
    {
        NX_ERROR(this, "Failed to get video params to reallocate surface: %1", sts);
        return sts;
    }
    mfxMemId inMid = nullptr;
    mfxMemId outMid = nullptr;

    surface.Info.CropW = param.mfx.FrameInfo.CropW;
    surface.Info.CropH = param.mfx.FrameInfo.CropH;
    m_mfxDecParams.mfx.FrameInfo.Width =
        MSDK_ALIGN16(std::max(param.mfx.FrameInfo.Width, m_mfxDecParams.mfx.FrameInfo.Width));
    m_mfxDecParams.mfx.FrameInfo.Height =
        MSDK_ALIGN16(std::max(param.mfx.FrameInfo.Height, m_mfxDecParams.mfx.FrameInfo.Height));
    surface.Info.Width = m_mfxDecParams.mfx.FrameInfo.Width;
    surface.Info.Height = m_mfxDecParams.mfx.FrameInfo.Height;

    inMid = surface.Data.MemId;

    sts = m_allocator->ReallocFrame(inMid, &surface.Info,
        surface.Data.MemType, &outMid);
    if (MFX_ERR_NONE != sts)
    {
        NX_ERROR(this, "Failed to reallocate surface, allocator failed: %1", sts);
        return false;
    }
    surface.Data.MemId = outMid;
    return true;
}

bool QuickSyncVideoDecoderImpl::allocFramesSample(mfxFrameAllocRequest& request)
{
    // alloc frames for decoder
    mfxStatus status = m_allocator->Alloc(m_allocator->pthis, &request, &m_response);
    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(this, "Alloc failed, status: %1", status);
        return false;
    }

    // prepare mfxFrameSurface1 array for decoder
    status = m_surfaces.AllocBuffers(m_response.NumFrameActual);
    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(this, "Alloc buffers failed, status: %1", status);
        return false;
    }

    for (int i = 0; i < m_response.NumFrameActual; i++)
    {
        // initating each frame:
        memcpy(&m_surfaces.m_pSurfaces[i].frame.Info, &(request.Info), sizeof(mfxFrameInfo));
        m_surfaces.m_pSurfaces[i].frame.Data.MemType = request.Type;
        m_surfaces.m_pSurfaces[i].frame.Data.MemId = m_response.mids[i];
    }
    return true;
}

bool QuickSyncVideoDecoderImpl::init(mfxBitstream& bitstream, AVCodecID codec)
{
    memset(&m_mfxDecParams, 0, sizeof(m_mfxDecParams));
    m_mfxDecParams.AsyncDepth = 4;
    if (m_config.useVideoMemory)
        m_mfxDecParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    else
        m_mfxDecParams.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

    if (codec == AV_CODEC_ID_H264)
    {
       m_mfxDecParams.mfx.CodecId = MFX_CODEC_AVC;
    }
    else if (codec == AV_CODEC_ID_H265)
    {
       m_mfxDecParams.mfx.CodecId = MFX_CODEC_HEVC;
    }
    else
    {
        NX_DEBUG(this, "Failed to init decoder, codec not supported: %1", codec);
        return false;
    }

    if (!initSession())
        return false;


    mfxStatus status = MFXVideoDECODE_DecodeHeader(m_mfxSession, &bitstream, &m_mfxDecParams);
    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(this, "Failed to decode video header, error: %1", status);
        return false;
    }

    if (!allocFrames())
    {
        NX_ERROR(this, "Failed to alloc frames, error");
        return false;
    }
    status = MFXVideoDECODE_Init(m_mfxSession, &m_mfxDecParams);
    if (MFX_WRN_PARTIAL_ACCELERATION == status) {
        NX_DEBUG(this, "Quick Sync partial acceleration!");
    } else if (status < MFX_ERR_NONE) {
        NX_ERROR(this, "Failed to init decoder, error: %1", status);
        return false;
    }
    //m_surfacePool.setAllocator(std::move(allocator));
    return true;
}

bool QuickSyncVideoDecoderImpl::syncOutputSurface(nx::QVideoFramePtr* result)
{
    if (m_outputSurfaces.empty())
    {
        NX_ERROR(this, "Empty output surface list");
        return false;
    }
    OutputSurface surface = m_outputSurfaces.front();
    m_outputSurfaces.pop_front();
    mfxStatus sts = MFX_WRN_IN_EXECUTION;
    while (MFX_WRN_IN_EXECUTION == sts) // TODO make hard limit
    {
        sts = m_mfxSession.SyncOperation(surface.syncp, kSyncWaitMsec);
        if (MFX_ERR_GPU_HANG == sts)
        {
            NX_DEBUG(this, "GPU hang happened");
            // Output surface can be corrupted
            // But should be delivered to output anyway
            sts = MFX_ERR_NONE;
        }
    }

    if (MFX_ERR_NONE != sts)
    {
        NX_ERROR(this, "Failed to sync surface: %1", sts);
        return false;
    }

    ++m_frameNumber;
    QAbstractVideoBuffer* buffer = nullptr;
    if (m_config.useVideoMemory)
    {
        vaapiMemId* surfaceData = (vaapiMemId*)surface.surface->frame.Data.MemId;
        VaSurfaceInfo surfaceDataHandle{*(surfaceData->m_surface), m_display};
        buffer = new MfxDrmQtVideoBuffer(surface.surface, surfaceDataHandle);
    }
    else
    {
        buffer = new MfxQtVideoBuffer(surface.surface, m_allocator);
    }
    QSize frameSize(surface.surface->frame.Info.Width, surface.surface->frame.Info.Height);
    result->reset(new QVideoFrame(buffer, frameSize, QVideoFrame::Format_NV12));
    result->get()->setStartTime(surface.surface->frame.Data.TimeStamp);
    return true;
}

int QuickSyncVideoDecoderImpl::decode(
    const QnConstCompressedVideoDataPtr& frame, nx::QVideoFramePtr* result)
{
    mfxBitstream bitstream;
    memset(&bitstream, 0, sizeof(bitstream));
    if (frame) // TODO make flush
    {
        if (m_bitstreamData.size() + frame->dataSize() > kMaxBitstreamSizeBytes)
        {
            NX_DEBUG(this, "Bitstream size too big: %1, clear ...", m_bitstreamData.size());
            m_bitstreamData.clear();
        }
        m_bitstreamData.insert(
            m_bitstreamData.end(), frame->data(), frame->data() + frame->dataSize());

        bitstream.Data = (mfxU8*)m_bitstreamData.data();
        bitstream.DataLength = m_bitstreamData.size();
        bitstream.MaxLength = m_bitstreamData.size();
        bitstream.TimeStamp = frame->timestamp;
    }
    else
        return 0;

    if (!m_mfxSession)
    {
        if (!frame)
        {
            NX_ERROR(this, "Fatal: failed to initialize decoder, empty frame");
            return -1;
        }

        if (!frame->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
        {
            m_bitstreamData.clear();
            return 0;
        }

        if (!init(bitstream, frame->compressionType))
        {
            NX_ERROR(this, "Failed to init quick sync video decoder");
            m_mfxSession.Close();
            return -1;
        }
    }

    mfxFrameSurface1* pOutSurface = nullptr;
    mfxBitstream* pBitstream = frame ? &bitstream : nullptr;
    mfxStatus sts = MFX_ERR_NONE;

    m_surfaces.SyncFrameSurfaces();
    if (!m_pCurrentFreeSurface)
        m_pCurrentFreeSurface = m_surfaces.m_FreeSurfacesPool.GetSurface();

    if (m_outputSurfaces.size() > kLatencySurfaceCount)
    {
        if (!syncOutputSurface(result))
            return -1;
    }

    OutputSurface outputSurface;
    do {
        sts = MFXVideoDECODE_DecodeFrameAsync(
            m_mfxSession,
            pBitstream,
            &(m_pCurrentFreeSurface->frame),
            &pOutSurface,
            &(outputSurface.syncp));
        if (MFX_WRN_DEVICE_BUSY != sts)
        {
            //NX_DEBUG(this, "QQQQ decode size result pBitstream->DataOffset %1, pBitstream->DataLength %2", pBitstream->DataOffset, pBitstream->DataLength);
            memmove(m_bitstreamData.data(), m_bitstreamData.data() + pBitstream->DataOffset, pBitstream->DataLength);
            pBitstream->DataOffset = 0;
            m_bitstreamData.resize(pBitstream->DataLength);
        }
        //NX_DEBUG(this, "QQQQ decode size result %1, sts %2", pBitstream->DataLength, sts);
        /*if (MFX_WRN_DEVICE_BUSY == sts && !syncOutputSurface(result))
        {
            NX_ERROR(this, "Device is busy and failed to free locked surface");
            return -1;
        }*/

        if (MFX_ERR_NONE == sts || MFX_ERR_MORE_SURFACE == sts)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (MFX_WRN_DEVICE_BUSY == sts || (pBitstream && pBitstream->DataLength > 3));


    if (sts > MFX_ERR_NONE) {
        // ignoring warnings...

        if (outputSurface.syncp)
        {
            MSDK_SELF_CHECK(pOutSurface);
            // output is available
            sts = MFX_ERR_NONE;
        }
        else
        {
            // output is not available
            sts = MFX_ERR_MORE_SURFACE;
        }
    }
    else if ((MFX_ERR_MORE_DATA == sts) && !pBitstream)
    {
        // Flush buffered surfaces.
        // TODO make flush
        //while (syncOutputSurface()) {}

        return m_frameNumber;
    }
    else if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == sts)
    {
        NX_ERROR(this, "Decode failed: %1", sts);
        return -1;
    }
    else if (MFX_ERR_REALLOC_SURFACE == sts)
    {
        if (!reallocCurrentSurface(m_pCurrentFreeSurface->frame))
            return -1;

        return m_frameNumber;
    }
    if ((MFX_ERR_NONE == sts) || (MFX_ERR_MORE_DATA == sts) || (MFX_ERR_MORE_SURFACE == sts))
    {
        // if current free surface is locked we are moving it to the used surfaces array
        if (m_pCurrentFreeSurface->frame.Data.Locked) {
            m_surfaces.m_UsedSurfacesPool.AddSurface(m_pCurrentFreeSurface);
            m_pCurrentFreeSurface = NULL;
        }
    }
    else if (MFX_ERR_NONE != sts)
    {
        NX_DEBUG(this, "DecodeFrameAsync returned error status %1", sts);
    }

    if (MFX_ERR_NONE == sts)
    {
        msdkFrameSurface* surface = (msdkFrameSurface*)(pOutSurface);
        msdk_atomic_inc16(&(surface->render_lock));
        outputSurface.surface = surface;
        m_outputSurfaces.push_back(outputSurface);
    }
    return m_frameNumber;
}

} // namespace nx::media
