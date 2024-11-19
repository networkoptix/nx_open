// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QMap>
#include <QtCore/QSize>

namespace nx {
namespace media {

class NX_MEDIA_API DecoderRegistrar
{
public:
    /**
     * Should be called once from main().
     *
     * @param maxFfmpegResolutions Limits applicability of the decoder. If empty, there is no limit.
     *     Map key with value 0 means default resolution limit otherwise limit for specified AV codec.
     * @param maxHardwareDecodersCount Maximum number of the simultaneously working hardware
     *     decoders.
     */
    static void registerDecoders(const QMap<int, QSize>& maxFfmpegResolutions,
        int maxHardwareDecodersCount = 1);
};

} // namespace media
} // namespace nx
