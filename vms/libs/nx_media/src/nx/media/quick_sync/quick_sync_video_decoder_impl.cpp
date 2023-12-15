// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "quick_sync_video_decoder_impl.h"

#include <deque>
#include <thread>

#include <sysmem_allocator.h>

#include <QtMultimedia/QVideoFrame>

#include <media/filters/h264_mp4_to_annexb.h>
#include <nx/codec/nal_units.h>
#include <nx/media/video_frame.h>
#include <nx/utils/log/log.h>

#include "compatibility_cache.h"
#include "mfx_status_string.h"
#include "mfx_sys_qt_video_buffer.h"
#include "qt_video_buffer.h"

namespace nx::media::quick_sync {

constexpr int kSyncWaitMsec = 5000;
constexpr int kMaxBitstreamSizeBytes = 1024 * 1024 * 10;

bool QuickSyncVideoDecoderImpl::isCompatible(
    const QnConstCompressedVideoDataPtr& frame, AVCodecID codec, int width, int height)
{
    return CompatibilityCache::isCompatible(frame, codec, width, height);
}

bool QuickSyncVideoDecoderImpl::isAvailable()
{
    auto isAvalibaleImpl = []() {
        MFXVideoSession mfxSession;
        mfxIMPL impl = MFX_IMPL_TYPE_HARDWARE;
        mfxVersion version = { {0, 1} };
        mfxStatus status = mfxSession.Init(impl, &version);
        if (status < MFX_ERR_NONE)
            return false;

    #if defined(__linux__)
        nx::media::quick_sync::DeviceContext device;
        if (!device.initialize(mfxSession, /*width*/0, /*height*/0))
            return false;
    #endif //__linux__
        return true;
    };

    static bool result = isAvalibaleImpl();
    return result;
}

QuickSyncVideoDecoderImpl::QuickSyncVideoDecoderImpl()
{
    memset(&m_response, 0, sizeof(m_response));
}

QuickSyncVideoDecoderImpl::~QuickSyncVideoDecoderImpl()
{
    NX_DEBUG(this, "Close quick sync video decoder");

    m_scaler.reset();
    m_mfxSession.Close();

    if (m_allocator)
        m_allocator->FreeFrames(&m_response);
}

bool QuickSyncVideoDecoderImpl::scaleFrame(
        mfxFrameSurface1* inputSurface, mfxFrameSurface1** outSurface, const QSize& targetSize)
{
    return m_scaler->scaleFrame(inputSurface, outSurface, targetSize);
}

bool QuickSyncVideoDecoderImpl::initSession()
{
    mfxIMPL impl = MFX_IMPL_AUTO_ANY;
    mfxVersion version = { {0, 1} };
    mfxStatus status = m_mfxSession.Init(impl, &version);
    if (status < MFX_ERR_NONE)
    {
        NX_WARNING(this, "Failed to init MFX session, error: %1", status);
        return false;
    }
    NX_DEBUG(this, "MFX version %1.%2", version.Major, version.Minor);
    return true;
}

bool QuickSyncVideoDecoderImpl::initDevice(int width, int height)
{
    if (!m_device.initialize(m_mfxSession, width, height))
    {
        NX_WARNING(this, "Failed to set handle to MFX session");
        return false;
    }

    // create memory allocator
    if (m_config.useVideoMemory)
        m_allocator = m_device.getAllocator();
    else
        m_allocator = std::make_shared<SysMemFrameAllocator>();

    m_mfxSession.SetFrameAllocator(m_allocator.get());
    return true;
}

bool QuickSyncVideoDecoderImpl::allocFrames()
{
    mfxStatus status = MFXVideoDECODE_Query(m_mfxSession, &m_mfxDecParams, &m_mfxDecParams);
    if (status < MFX_ERR_NONE)
    {
        NX_WARNING(this, "Query failed, error: %1", status);
        return false;
    }

    mfxFrameAllocRequest request;
    memset(&request, 0, sizeof(request));
    status = MFXVideoDECODE_QueryIOSurf(m_mfxSession, &m_mfxDecParams, &request);
    if (status < MFX_ERR_NONE)
    {
        NX_WARNING(this, "QueryIOSurf failed, error: %1", status);
        return false;
    }

    // Add surfaces for rendering smoothness
    constexpr int kMaxFps = 30;
    request.NumFrameSuggested += kMaxFps / 5;

    if (request.NumFrameSuggested < m_mfxDecParams.AsyncDepth)
    {
        NX_WARNING(this, "NumFrameSuggested(%1) less then AsyncDepth(%2)",
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
        NX_WARNING(this, "Alloc failed, status: %1", status);
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
    mfxBitstream& bitstream, const QnConstCompressedVideoDataPtr& frame)
{
    if (!frame)
    {
        NX_WARNING(this, "Failed to initialize decoder, empty frame");
        return false;
    }

    if (!frame->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
        return false;

    memset(&m_mfxDecParams, 0, sizeof(m_mfxDecParams));
    m_mfxDecParams.AsyncDepth = 4;
    if (m_config.useVideoMemory)
        m_mfxDecParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    else
        m_mfxDecParams.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

    if (frame->compressionType == AV_CODEC_ID_H264)
    {
       m_mfxDecParams.mfx.CodecId = MFX_CODEC_AVC;
    }
    else if (frame->compressionType == AV_CODEC_ID_H265)
    {
       m_mfxDecParams.mfx.CodecId = MFX_CODEC_HEVC;
    }
    else
    {
        NX_DEBUG(this, "Failed to init decoder, codec not supported: %1", frame->compressionType);
        return false;
    }

    if (!initSession())
        return false;

    mfxStatus status = MFXVideoDECODE_DecodeHeader(m_mfxSession, &bitstream, &m_mfxDecParams);
    if (status < MFX_ERR_NONE)
    {
        NX_WARNING(this, "Failed to decode video header, error: %1", status);
        return false;
    }
    if (!initDevice(m_mfxDecParams.mfx.FrameInfo.Width, m_mfxDecParams.mfx.FrameInfo.Height))
        return false;

    if (!allocFrames())
    {
        NX_WARNING(this, "Failed to alloc frames, error");
        return false;
    }
    status = MFXVideoDECODE_Init(m_mfxSession, &m_mfxDecParams);
    if (MFX_WRN_PARTIAL_ACCELERATION == status) {
        NX_DEBUG(this, "Quick Sync partial acceleration!");
    } else if (status < MFX_ERR_NONE) {
        NX_WARNING(this, "Failed to init decoder, error: %1", status);
        return false;
    }

#ifndef Q_OS_LINUX
    m_scaler = std::make_unique<VppScaler>(m_mfxSession, m_allocator);
    // Initialize the scaler to reserve the maximum size, since VPP does not support reset the
    // target size up
    QSize frameSize(m_mfxDecParams.mfx.FrameInfo.Width, m_mfxDecParams.mfx.FrameInfo.Height);
    return m_scaler->init(frameSize, frameSize);
#else
    return true;
#endif
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
    mfxFrameSurface1* surface, nx::media::VideoFramePtr* result)
{
    QAbstractVideoBuffer* buffer = nullptr;
    if (m_config.useVideoMemory)
        buffer = new QtVideoBuffer(QuickSyncSurface{surface, weak_from_this()});
    else
        buffer = new MfxQtVideoBuffer(surface, m_allocator);

    QSize frameSize(surface->Info.CropW, surface->Info.CropH);
    result->reset(new VideoFrame(buffer, {frameSize, QVideoFrameFormat::Format_NV12}));
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

std::unique_ptr<mfxBitstream> QuickSyncVideoDecoderImpl::updateBitStream(
    const QnConstCompressedVideoDataPtr& frame)
{
    std::unique_ptr<mfxBitstream> bitstream;
    if (frame)
    {
        if (m_bitstreamData.size() + frame->dataSize() > kMaxBitstreamSizeBytes)
        {
            NX_DEBUG(this, "Bitstream size too big: %1, clear ...", m_bitstreamData.size());
            clearData();
        }
        m_bitstreamData.insert(m_bitstreamData.end(), frame->data(),
            frame->data() + frame->dataSize());

        bitstream = std::make_unique<mfxBitstream>();
        memset(bitstream.get(), 0, sizeof(mfxBitstream));
        bitstream->Data = (mfxU8*)m_bitstreamData.data();
        bitstream->DataLength = m_bitstreamData.size();
        bitstream->MaxLength = m_bitstreamData.size();
    }
    else
    {
        clearData();
    }

    return bitstream;
}

void QuickSyncVideoDecoderImpl::resetDecoder()
{
    if (m_decoderInitialized)
        MFXVideoDECODE_Reset(m_mfxSession, &m_mfxDecParams);
    clearData();
}

int QuickSyncVideoDecoderImpl::decode(
    const QnConstCompressedVideoDataPtr& frame, nx::media::VideoFramePtr* result)
{
    QnConstCompressedVideoDataPtr frameAnnexB;
    if (frame)
    {
        // The filter keeps same frame in case of filtering is not required or
        // it is not the H264 codec.
        H2645Mp4ToAnnexB filter;
        frameAnnexB = std::dynamic_pointer_cast<const QnCompressedVideoData>(
            filter.processData(frame));
        if (!frameAnnexB)
        {
            NX_WARNING(this, "Failed to convert h264 video to Annex B format");
            return 0;
        }
    }

    std::unique_ptr<mfxBitstream> bitstream = updateBitStream(frameAnnexB);
    if (!m_mfxSession && !init(*bitstream, frameAnnexB))
    {
        NX_WARNING(this, "Failed to init quick sync video decoder");
        m_mfxSession.Close();
        clearData();
        return -1;
    }
    m_decoderInitialized = true;

    mfxStatus status = MFX_ERR_NONE;
    mfxSyncPoint syncp;
    mfxFrameSurface1* outSurface = nullptr;

    // QS decoder could keep frame in our buffer due to b-frames
    if (frame)
    {
        m_dtsQueue.push_back(frame->timestamp);
        bitstream->TimeStamp = m_dtsQueue.front();
    }

    mfxFrameSurface1* surface = nullptr;
    bool isWarning = false;
    do
    {
        if (!surface)
        {
            surface = getFreeSurface();
            if (!surface)
            {
                NX_WARNING(this, "Failed to decode frame, no free surfaces!");
                return -1;
            }
        }

        status = MFXVideoDECODE_DecodeFrameAsync(
            m_mfxSession, bitstream.get(), surface, &outSurface, &syncp);

        if (MFX_WRN_DEVICE_BUSY == status)
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Wait if device is busy

        isWarning = (status > MFX_ERR_NONE) && !syncp;
        if (status == MFX_ERR_MORE_SURFACE)
        {
            surface = nullptr;
            isWarning = true;
        }
    } while (MFX_WRN_DEVICE_BUSY == status || isWarning);

    if (bitstream && bitstream->DataOffset > 0)
    {
        if (!m_dtsQueue.empty())
            m_dtsQueue.pop_front();

        memmove(m_bitstreamData.data(),
                m_bitstreamData.data() + bitstream->DataOffset, bitstream->DataLength);
        bitstream->DataOffset = 0;
        m_bitstreamData.resize(bitstream->DataLength);
    }

    // Ignore warnings if output is available (see intel media sdk samples)
    if (MFX_ERR_NONE < status && syncp)
        status = MFX_ERR_NONE;

    if (MFX_ERR_NONE != status)
    {
        if (status < MFX_ERR_NONE && status != MFX_ERR_MORE_DATA)
            NX_WARNING(this, "DecodeFrameAsync failed, error: %1", status);
        return m_frameNumber;
    }

    // Synchronize. Wait until decoded frame is ready
    status = m_mfxSession.SyncOperation(syncp, kSyncWaitMsec);

    if (MFX_ERR_NONE != status)
    {
        NX_WARNING(this, "Failed to sync surface, error: %1", status);
        return -1;
    }
    if (!buildQVideoFrame(outSurface, result))
        return -1;

    return m_frameNumber++;
}

} // namespace nx::media::quick_sync
