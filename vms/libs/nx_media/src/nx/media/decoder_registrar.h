#pragma once

#include <memory>

#include <QtCore/QSize>
#include <QtCore/QMap>

namespace nx {
namespace media {

class DecoderRegistrar
{
public:
    struct Config
    {
        /*
        * @param maxFfmpegResolutions Limits applicability of the decoder. If empty, there is no
        * limit. Map key with value 0 means default resolution limit otherwise limit for specified
        * AV codec.
        */
        QMap<int, QSize> maxFfmpegResolutions;
        bool isTranscodingEnabled = true;
        bool enableHardwareDecoder = true;
    };
public:
    /**
     * Should be called once from main().
     */
    static void registerDecoders(const Config config);
};

} // namespace media
} // namespace nx
