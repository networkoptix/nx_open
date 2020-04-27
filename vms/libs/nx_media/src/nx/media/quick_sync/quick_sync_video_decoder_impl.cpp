
#include "quick_sync_video_decoder_impl.h"

#include <deque>
#include <thread>

#include <QtMultimedia/QVideoFrame>

#include <utils/media/nalUnits.h>
#include <nx/utils/log/log.h>

#include "allocators/sysmem_allocator.h"
#include "mfx_sys_qt_video_buffer.h"
#include "qt_video_buffer.h"
#include "mfx_status_string.h"

#define MSDK_ALIGN16(value) (((value + 15) >> 4) << 4) // round up to a multiple of 16

namespace nx::media::quick_sync {

constexpr int kSyncWaitMsec = 5000;
constexpr int kMaxBitstreamSizeBytes = 1024 * 1024 * 10;

bool QuickSyncVideoDecoderImpl::isCompatible(AVCodecID codec)
{
    return nx::media::quick_sync::isCompatible(codec);
}

QuickSyncVideoDecoderImpl::QuickSyncVideoDecoderImpl()
{
    memset(&m_response, 0, sizeof(m_response));
}

QuickSyncVideoDecoderImpl::~QuickSyncVideoDecoderImpl()
{
    NX_DEBUG(this, "Close quick sync video decoder");

    m_mfxSession.Close();

    if (m_allocator)
        m_allocator->FreeFrames(&m_response);
}

bool QuickSyncVideoDecoderImpl::initSession()
{
    mfxIMPL impl = MFX_IMPL_AUTO_ANY;
    mfxVersion version = { {0, 1} };
    mfxStatus status = m_mfxSession.Init(impl, &version);
    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(this, "Failed to init MFX session, error: %1", toString(status));
        return false;
    }
    NX_DEBUG(this, "MFX version %1.%2", version.Major, version.Minor);

    if (!m_device.initialize(m_mfxSession))
    {
        NX_ERROR(this, "Failed to set handle to MFX session");
        return false;
    }

    // create memory allocator
    if (m_config.useVideoMemory)
    {
        m_allocator = m_device.getAllocator();
    }
    else
    {
        auto allocator = std::make_shared<SysMemFrameAllocator>();
        status = allocator->Init(nullptr);
        if (status < MFX_ERR_NONE)
        {
            NX_ERROR(this, "Failed to init allocator, error code: %1", toString(status));
            return false;
        }
        m_allocator = std::move(allocator);
    }
    m_mfxSession.SetFrameAllocator(m_allocator.get());
    return true;
}

bool QuickSyncVideoDecoderImpl::allocFrames()
{
    mfxStatus status = MFXVideoDECODE_Query(m_mfxSession, &m_mfxDecParams, &m_mfxDecParams);
    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(this, "Query failed, error: %1", toString(status));
        return false;
    }

    mfxFrameAllocRequest request;
    memset(&request, 0, sizeof(request));
    status = MFXVideoDECODE_QueryIOSurf(m_mfxSession, &m_mfxDecParams, &request);
    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(this, "QueryIOSurf failed, error: %1", toString(status));
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
    return allocSurfaces(request);
}

bool QuickSyncVideoDecoderImpl::allocSurfaces(mfxFrameAllocRequest& request)
{
    // alloc frames for decoder
    mfxStatus status = m_allocator->Alloc(m_allocator->pthis, &request, &m_response);
    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(this, "Alloc failed, toString(status): %1", toString(status));
        return false;
    }

    // Allocate surface headers (mfxFrameSurface1) for decoder
    m_surfaces = std::vector<SurfaceInfo>(m_response.NumFrameActual);
    for (int i = 0; i < m_response.NumFrameActual; i++)
    {
        memset(&m_surfaces[i].surface, 0, sizeof(mfxFrameSurface1));
        m_surfaces[i].surface.Info = request.Info;

        // MID (memory id) represents one video NV12 surface
        m_surfaces[i].surface.Data.MemId = m_response.mids[i];
        m_surfaces[i].surface.Data.MemType = request.Type;
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
        NX_ERROR(this, "Failed to decode video header, error: %1", toString(status));
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
        NX_ERROR(this, "Failed to init decoder, error: %1", toString(status));
        return false;
    }
    return true;
}

void QuickSyncVideoDecoderImpl::lockSurface(const mfxFrameSurface1* surface)
{
    for (auto& surfaceInfo: m_surfaces)
    {
        if (surface == &surfaceInfo.surface)
        {
            surfaceInfo.isUsed = true;
            return;
        }
    }
}

void QuickSyncVideoDecoderImpl::releaseSurface(const mfxFrameSurface1* surface)
{
    for (auto& surfaceInfo: m_surfaces)
    {
        if (surface == &surfaceInfo.surface)
        {
            surfaceInfo.isUsed = false;
            return;
        }
    }
}

bool QuickSyncVideoDecoderImpl::buildQVideoFrame(
    mfxFrameSurface1* surface, nx::QVideoFramePtr* result)
{
    QAbstractVideoBuffer* buffer = nullptr;
    if (m_config.useVideoMemory)
    {
        buffer = new QtVideoBuffer(QuickSyncSurface{surface, weak_from_this()});
    }
    else
        buffer = new MfxQtVideoBuffer(surface, m_allocator);

    QSize frameSize(surface->Info.Width, surface->Info.Height);
    result->reset(new QVideoFrame(buffer, frameSize, QVideoFrame::Format_NV12));
    result->get()->setStartTime(surface->Data.TimeStamp);

    //if (!getDevice().device.getRenderer().render(surface))
     //   return false;
    return true;
}

mfxFrameSurface1* QuickSyncVideoDecoderImpl::getFreeSurface()
{
    for (auto& surfaceInfo: m_surfaces)
    {
        if (0 == surfaceInfo.surface.Data.Locked && !surfaceInfo.isUsed)
            return &surfaceInfo.surface;
    }
    return nullptr;
}

int QuickSyncVideoDecoderImpl::decode(
    const QnConstCompressedVideoDataPtr& frame, nx::QVideoFramePtr* result)
{
    NX_INFO(this, "Decode");
    mfxBitstream bitstream;
    memset(&bitstream, 0, sizeof(bitstream));
    if (frame)
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
    } else
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

    mfxBitstream* pBitstream = frame ? &bitstream : nullptr;
    mfxStatus status = MFX_ERR_NONE;

    mfxFrameSurface1* surface = getFreeSurface();
    if (!surface)
    {
        NX_ERROR(this, "Failed to decode frame, no free surfaces!");
        return -1;
    }
    mfxSyncPoint syncp;
    mfxFrameSurface1* outSurface = nullptr;

    do
    {
        status = MFXVideoDECODE_DecodeFrameAsync(
            m_mfxSession, pBitstream, surface, &outSurface, &syncp);

        if (MFX_WRN_DEVICE_BUSY == status)
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Wait if device is busy

    } while (MFX_WRN_DEVICE_BUSY == status);

    if (pBitstream)
    {
        memmove(m_bitstreamData.data(),
                m_bitstreamData.data() + pBitstream->DataOffset, pBitstream->DataLength);
        pBitstream->DataOffset = 0;
        m_bitstreamData.resize(pBitstream->DataLength);
    }

    // Ignore warnings if output is available (see intel media sdk samples)
    if (MFX_ERR_NONE < status && syncp)
        status = MFX_ERR_NONE;

    if (MFX_ERR_NONE != status)
    {
        if (status < MFX_ERR_NONE && status != MFX_ERR_MORE_DATA)
            NX_ERROR(this, "DecodeFrameAsync failed, error status: %1", toString(status));
        return m_frameNumber;
    }

    // Synchronize. Wait until decoded frame is ready
    status = m_mfxSession.SyncOperation(syncp, kSyncWaitMsec);

    if (MFX_ERR_NONE != status)
    {
        NX_ERROR(this, "Failed to sync surface: %1", toString(status));
        return -1;
    }
    if (!buildQVideoFrame(outSurface, result))
        return -1;

    NX_INFO(this, "New frame");
    return m_frameNumber++;
}

} // namespace nx::media::quick_sync
