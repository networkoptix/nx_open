// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>

#include <nx/sdk/interface.h>

#include "i_data_packet.h"
#include "i_media_context.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Packet containing compressed media data (audio or video).
 */
class ICompressedMediaPacket: public Interface<ICompressedMediaPacket, IDataPacket>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::ICompressedMediaPacket"); }

    /**
     * @return Null-terminated ASCII string containing the MIME type corresponding to the packet
     * data compression.
     */
    virtual const char* codec() const = 0;

    /**
     * @return Pointer to the compressed data.
     */
    virtual const char* data() const = 0;

    /**
     * @return Size of the compressed data in bytes.
     */
    virtual int dataSize() const = 0;

    /**
     * @return Pointer to the codec context, or null if not available.
     */
    protected: virtual const IMediaContext* getContext() const = 0;
    public: Ptr<const IMediaContext> context() const { return toPtr(getContext()); }

    enum class MediaFlags: uint32_t
    {
        none = 0,
        keyFrame = 1 << 0,
        all = UINT32_MAX
    };

    /**
     * @return Packet media flags.
     */
    virtual MediaFlags flags() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
