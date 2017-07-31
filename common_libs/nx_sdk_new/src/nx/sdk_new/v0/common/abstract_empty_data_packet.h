#pragma once

#include <nx/sdk_new/v0/common/abstract_data_packet.h>

namespace nx {
namespace sdk {
namespace v0 {

class AbstractEmptyDataPacket: public AbstractDataPacket
{
    // TODO: #dmishin ServiceDataPacket? EOS EOB etc.
};

} // namespace v0
} // namespace sdk
} // namespace nx
