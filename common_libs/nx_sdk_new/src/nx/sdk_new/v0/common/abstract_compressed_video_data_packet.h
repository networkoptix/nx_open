#pragma once

#include <nx/sdk_new/v0/common/abstract_compressed_media_data_packet.h>

namespace nx {
namespace sdk {
namespace v0 {

class AbstractCompressedVideoDataPacket: public AbstractCompressedMediaDataPacket
{
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual void sampleAspectRatio(int* outNumerator, int* outDenumerator) const = 0;
    virtual nx::sdk::v0::ColorRange colorRange() const = 0;
    virtual nx::sdk::v0::ColorPrimaries colorPrimaries() = 0;
    virtual nx::sdk::v0::ColorTrc colorTrc() = 0;

};

} // namespace v0
} // namespace sdk
} // namespace nx
