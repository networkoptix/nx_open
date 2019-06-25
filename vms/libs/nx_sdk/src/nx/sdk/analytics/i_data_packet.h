// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>

#include <nx/sdk/interface.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Base class for classes that represent the packet of data (e.g. audio, video, metadata).
 */
class IDataPacket: public Interface<IDataPacket>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::IDataPacket"); }

    /**
     * @return Timestamp of the media data in microseconds since epoch, or 0 if not relevant.
     */
    virtual int64_t timestampUs() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
