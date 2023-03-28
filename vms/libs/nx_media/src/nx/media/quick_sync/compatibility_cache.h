// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QSize>
#include <deque>
#include <map>
#include <mutex>
#include <vector>

extern "C"
{
#include <libavcodec/avcodec.h>

} // extern "C"

#include <nx/media/video_data_packet.h>

class CompatibilityCache
{
public:
    static bool isCompatible(
        const QnConstCompressedVideoDataPtr& frame, AVCodecID codec, int width, int height)
    {
        static CompatibilityCache table;
        return table.isCompatibleCached(frame, codec, width, height);
    }

private:
    bool isCompatibleCached(
        const QnConstCompressedVideoDataPtr& frame, AVCodecID codec, int width, int height);
    bool isDecodableCached(const QnConstCompressedVideoDataPtr& frame);
    bool isResolutionCompatibleCached(AVCodecID codec, int width, int height);
    bool tryResolutionCompatibility(AVCodecID codec, int width, int height);

private:
    std::mutex m_mutex;
    std::map<AVCodecID, QSize> m_compatibleSize;
    std::map<AVCodecID, QSize> m_uncompatibleSize;
    std::deque<std::pair<std::vector<uint8_t>, bool>> m_extradataCompatibility;
};
