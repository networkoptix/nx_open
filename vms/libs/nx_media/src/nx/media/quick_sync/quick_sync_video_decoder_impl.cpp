
#include "quick_sync_video_decoder_impl.h"

#include <deque>
#include <thread>

#include <QtMultimedia/QVideoFrame>

#include <utils/media/nalUnits.h>
#include <utils/media/utils.h>
#include <nx/utils/log/log.h>

#include "allocators/sysmem_allocator.h"
#include "mfx_sys_qt_video_buffer.h"
#include "qt_video_buffer.h"
#include "mfx_status_string.h"
#include "compatibility_cache.h"
#include <media/filters/h264_mp4_to_annexb.h>

#define MSDK_ALIGN16(value) (((value + 15) >> 4) << 4) // round up to a multiple of 16

namespace nx::media::quick_sync {

constexpr int kSyncWaitMsec = 5000;
constexpr int kMaxBitstreamSizeBytes = 1024 * 1024 * 10;

bool QuickSyncVideoDecoderImpl::isCompatible(AVCodecID codec, int width, int height)
{
    return CompatibilityCache::isCompatible(codec, width, height);
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

bool QuickSyncVideoDecoderImpl::initSession(int width, int height)
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

    if (!m_device.initialize(m_mfxSession, width, height))
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
            NX_ERROR(this, "Failed to init allocator, error code: %1", status);
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
    return allocSurfaces(request);
}

bool QuickSyncVideoDecoderImpl::allocSurfaces(mfxFrameAllocRequest& request)
{
    // alloc frames for decoder
    mfxStatus status = m_allocator->Alloc(m_allocator->pthis, &request, &m_response);
    if (status < MFX_ERR_NONE)
    {
        NX_ERROR(this, "Alloc failed, status: %1", status);
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

bool QuickSyncVideoDecoderImpl::init(
    mfxBitstream& bitstream, AVCodecID codec, int width, int height)
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

    if (!initSession(width, height))
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
        buffer = new QtVideoBuffer(QuickSyncSurface{surface, weak_from_this()});

    else
        buffer = new MfxQtVideoBuffer(surface, m_allocator);

    QSize frameSize(surface->Info.Width, surface->Info.Height);
    result->reset(new QVideoFrame(buffer, frameSize, QVideoFrame::Format_NV12));
    result->get()->setStartTime(surface->Data.TimeStamp);
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

void QuickSyncVideoDecoderImpl::clearData()
{
    m_bitstreamData.clear();
    m_dtsQueue.clear();
}

int QuickSyncVideoDecoderImpl::decode(
    const QnConstCompressedVideoDataPtr& frame, nx::QVideoFramePtr* result)
{
    if (!frame)
        return 0; // TODO flush decoder

    mfxBitstream bitstream;
    memset(&bitstream, 0, sizeof(bitstream));

    if (m_bitstreamData.size() + frame->dataSize() > kMaxBitstreamSizeBytes)
    {
        NX_DEBUG(this, "Bitstream size too big: %1, clear ...", m_bitstreamData.size());
        clearData();
    }

    // The filter keeps same frame in case of filtering is not required or
    // it is not the H264 codec.
    H264Mp4ToAnnexB filter;
    auto filteredFrame = std::dynamic_pointer_cast<const QnCompressedVideoData>(
        filter.processData(frame));
    if (!NX_ASSERT(filteredFrame))
        return 0;
    m_bitstreamData.insert(m_bitstreamData.end(), filteredFrame->data(),
        filteredFrame->data() + filteredFrame->dataSize());

    bitstream.Data = (mfxU8*)m_bitstreamData.data();
    bitstream.DataLength = m_bitstreamData.size();
    bitstream.MaxLength = m_bitstreamData.size();

    if (!m_mfxSession)
    {
        if (!frame)
        {
            NX_ERROR(this, "Fatal: failed to initialize decoder, empty frame");
            return -1;
        }

        if (!frame->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
        {
            clearData();
            return 0;
        }

        QSize size = getFrameSize(frame, true);
        if (!size.isValid())
        {
            NX_ERROR(this, "Failed to init quick sync video decoder, frame size unknown");
            return -1;
        }

        if (!init(bitstream, frame->compressionType, MSDK_ALIGN16(size.width()), MSDK_ALIGN16(size.height())))
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

    m_dtsQueue.push_back(frame->timestamp);
    bitstream.TimeStamp = m_dtsQueue.front();

    bool isWarning = false;
    do
    {
        status = MFXVideoDECODE_DecodeFrameAsync(
            m_mfxSession, pBitstream, surface, &outSurface, &syncp);

        if (MFX_WRN_DEVICE_BUSY == status)
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Wait if device is busy

        isWarning = (status > MFX_ERR_NONE) && !syncp;
    } while (MFX_WRN_DEVICE_BUSY == status || isWarning);

    if (pBitstream && pBitstream->DataOffset > 0)
    {
        // QS decoder could keep frame in our buffer due to b-frames
        m_dtsQueue.pop_front();

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
            NX_ERROR(this, "DecodeFrameAsync failed, error: %1", status);
        return m_frameNumber;
    }

    // Synchronize. Wait until decoded frame is ready
    status = m_mfxSession.SyncOperation(syncp, kSyncWaitMsec);

    if (MFX_ERR_NONE != status)
    {
        NX_ERROR(this, "Failed to sync surface, error: %1", status);
        return -1;
    }
    if (!buildQVideoFrame(outSurface, result))
        return -1;

    return m_frameNumber++;
}

} // namespace nx::media::quick_sync
