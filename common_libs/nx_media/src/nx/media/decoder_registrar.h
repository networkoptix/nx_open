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
     * @param maxResolution Limits applicability of the decoder. If empty, there is no limit.
     * Map key with value 0 means default resolution limit otherwise limit for specified AV codec.
     * @param liteMode Whether the library is used in Mobile Client in Lite mode.
     */
    static void registerDecoders(
        std::shared_ptr<AbstractResourceAllocator> allocator,
        const QMap<int, QSize>& maxFfmpegResolutions,
        bool isTranscodingEnabled);
};

} // namespace media
} // namespace nx
