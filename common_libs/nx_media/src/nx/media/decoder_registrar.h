#pragma once

#include <memory>

#include <QtCore/QSize>

#include "abstract_resource_allocator.h"

namespace nx {
namespace media {

class DecoderRegistrar
{
public:
    /**
     * Should be called once from main().
     *
     * @param maxFfmpegResolution Limits applicability of ffmpeg software decoder. If empty, there
     * is no limit.
     *
     * @param liteMode Whether the library is used in Mobile Client in Lite mode.
     */
    static void registerDecoders(
        std::shared_ptr<AbstractResourceAllocator> allocator,
        const QSize& maxFfmpegResolution,
        bool liteClientMode);
};

} // namespace media
} // namespace nx
