#pragma once

#include <nx/sdk_new/v0/common/abstract_object.h>

namespace nx {
namespace sdk {
namespace v0 {

enum class DataPacketType
{
    unknown = 0,
    video,
    compressedVideo,
    audio,
    compressedAudio,
    meta,
    empty
};

class AbstractDataPacket: public AbstractObject
{
    virtual nx::sdk::v0::Guid vendorFormatGuid() const = 0;

    virtual DataPacketType type() const = 0;

    virtual int64_t timestamp() const = 0;

    virtual void* data() const = 0;

    virtual int dataSize() const = 0;

    virtual int channel() const = 0;

    virtual int duration() const = 0;
};

} // namespace v0
} // namespace sdk
} // namespace nx
