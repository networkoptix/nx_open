#pragma once

#include <plugins/metadata/abstract_data_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

static const nxpl::GUID IID_MediaFrame
    = {{0x13, 0x85, 0x3c, 0xd6, 0x13, 0x7e, 0x4d, 0x8b, 0x9b, 0x8e, 0x63, 0xf1, 0x5f, 0x93, 0x2a, 0xc1}};

class AbstractMediaFrame: public AbstractDataPacket
{
public:
    virtual const char** data() const = 0;
    virtual int planeCount() const = 0;
    virtual int* planeSize() const = 0;
};

} // namespace metadata
} // namesapce sdk
} // namespace nx
