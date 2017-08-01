#pragma once

#include <plugins/plugin_api.h>
#include <plugins/metadata/abstract_data_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

static const nxpl::GUID IID_MediaPacket
    = {{0xf9, 0xa4, 0x59, 0x8b, 0xd7, 0x18, 0x42, 0x29, 0x98, 0xdd, 0xff, 0xe5, 0x41, 0x28, 0xfa, 0xf8}};

class AbstractCompressedMediaPacket: public AbstractDataPacket
{
public:
    virtual const char* codec() const = 0;

    virtual const char* data() const = 0;

    virtual const int dataSize() const = 0;

    // ??? Context paramters
};

} // namespace metadata
} // namespace sdk
} // namespace nx
