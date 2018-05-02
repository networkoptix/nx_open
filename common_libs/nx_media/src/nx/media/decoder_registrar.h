#pragma once

#include <memory>

#include <QtCore/QSize>

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
        const QSize& maxFfmpegResolution,
        bool isTranscodingEnabled);
};

} // namespace media
} // namespace nx
