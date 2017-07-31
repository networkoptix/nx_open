#pragma once

#include <nx/sdk_new/v0/common/abstract_data_packet.h>
#include <nx/sdk_new/v0/common/codec.h>

namespace nx {
namespace sdk {
namespace v0 {

class AbstractCompressedMediaDataPacket: public AbstractDataPacket
{
    virtual nx::sdk::v0::Codec codec() const = 0;
    virtual nx::sdk::v0::CodecTag codecTag() const = 0;
    virtual uint8_t* extradata() const = 0;
    virtual int extradataSize() const = 0;
    virtual int profile() const = 0;
    virtual int level() const = 0;
    virtual int64_t bitRate() const = 0;
    virtual int bitsPerCodedSample() const = 0;
    virtual int bitsPerRawSample() const = 0;

    // TODO: #dmishin think better, see AVCodecContext and AVCodecParamters
};

} // namespace v0
} // namespace sdk
} // namespace nx
