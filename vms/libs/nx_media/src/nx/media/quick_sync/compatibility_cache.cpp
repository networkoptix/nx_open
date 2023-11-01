// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "compatibility_cache.h"

#include <mfx/mfxvideo++.h>
#include <nx/media/utils.h>

#include "quick_sync_video_decoder_impl.h"
#include "utils.h"

#define MSDK_ALIGN16(value) (((value + 15) >> 4) << 4) // round up to a multiple of 16

namespace {

constexpr int kMaxCachedExtradata = 256;

}

bool CompatibilityCache::isCompatibleCached(
    const QnConstCompressedVideoDataPtr& frame, AVCodecID codec, int width, int height)
{
    if (codec != AV_CODEC_ID_H264 && codec != AV_CODEC_ID_HEVC)
        return false;

    std::scoped_lock<std::mutex> lock(m_mutex);
    if (!isResolutionCompatibleCached(codec, width, height))
        return false;

    return isDecodableCached(frame);
}

bool CompatibilityCache::isDecodableCached(const QnConstCompressedVideoDataPtr& frame)
{
    if (!frame) //< TODO temporary ignore empty frame for new player interface
        return true;

    auto extraData = nx::media::buildExtraDataAnnexB(frame);
    if (extraData.empty())
        return true;

    for (const auto& data: m_extradataCompatibility)
    {
        if (data.first == extraData)
            return data.second;
    }

    // Try to decode frame.
    bool isDecodable = true;
    nx::media::quick_sync::QuickSyncVideoDecoderImpl decoder;
    nx::media::VideoFramePtr result;
    if (decoder.decode(frame, &result) < 0)
        isDecodable = false;

    m_extradataCompatibility.push_back(std::make_pair(extraData, isDecodable));
    if (m_extradataCompatibility.size() > kMaxCachedExtradata)
        m_extradataCompatibility.pop_front();

    return isDecodable;
}

bool CompatibilityCache::isResolutionCompatibleCached(AVCodecID codec, int width, int height)
{
    auto compatibleSize = m_compatibleSize.find(codec);
    if (compatibleSize != m_compatibleSize.end()
        && compatibleSize->second.width() >= width
        && compatibleSize->second.height() >= height)
    {
        return true;
    }
    auto uncompatibleSize = m_uncompatibleSize.find(codec);
    if (uncompatibleSize != m_uncompatibleSize.end()
        && uncompatibleSize->second.width() <= width
        && uncompatibleSize->second.height() <= height)
    {
        return false;
    }
    bool isCompatible = tryResolutionCompatibility(codec, width, height);
    if (isCompatible)
        m_compatibleSize[codec] = QSize(width, height);
    else
        m_uncompatibleSize[codec] = QSize(width, height);
    return isCompatible;
}

bool CompatibilityCache::tryResolutionCompatibility(AVCodecID codec, int width, int height)
{
    mfxVideoParam param;
    memset(&param, 0, sizeof(param));

    param.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    if (codec == AV_CODEC_ID_H264)
    {
        param.mfx.CodecId = MFX_CODEC_AVC;
        param.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH; // TODO ?
        param.mfx.CodecLevel = MFX_LEVEL_AVC_41;
    }
    else if (codec == AV_CODEC_ID_H265)
    {
        param.mfx.CodecId = MFX_CODEC_HEVC;
        param.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
        param.mfx.CodecLevel = MFX_LEVEL_HEVC_4;
        param.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    }
    else
    {
        return false;
    }
    param.mfx.FrameInfo.Width = MSDK_ALIGN16(width);
    param.mfx.FrameInfo.Height = MSDK_ALIGN16(height);
    param.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    param.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

    MFXVideoSession mfxSession;
    mfxIMPL impl = MFX_IMPL_AUTO_ANY;
    mfxVersion version = { {0, 1} };
    mfxStatus status = mfxSession.Init(impl, &version);
    if (status < MFX_ERR_NONE)
        return false;

    nx::media::quick_sync::DeviceContext device;
    if (!device.initialize(mfxSession, width, height))
        return false;

    status = MFXVideoDECODE_Query(mfxSession, &param, &param);
    if (status < MFX_ERR_NONE)
        return false;

    status = MFXVideoDECODE_Init(mfxSession, &param);
    if (status < MFX_ERR_NONE)
        return false;

    MFXVideoDECODE_Close(mfxSession);
    return true;
}
