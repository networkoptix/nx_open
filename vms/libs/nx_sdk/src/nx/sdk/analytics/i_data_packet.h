// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <stdint.h>

#include <nx/kit/flags.h>
#include <nx/sdk/interface.h>

namespace nx::sdk::analytics {

/**
 * Base class for classes that represent the packet of data (e.g. audio, video, metadata).
 */
class IDataPacket: public Interface<IDataPacket>
{
public:
    enum class Flags: uint32_t
    {
        none = 0,

        /**
         * Indicates that the timestamp is sampled from a camera's own internal clock and should
         * be translated to VMS System clock.
         */
        cameraClockTimestamp = 1 << 0,
    };

public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IDataPacket"); }

    /**
     * @return Timestamp of the media data in microseconds since epoch, or 0 if not relevant.
     */
    virtual int64_t timestampUs() const = 0;
};
using IDataPacket0 = IDataPacket;

NX_KIT_ENABLE_FLAGS(IDataPacket::Flags);

} // namespace nx::sdk::analytics
