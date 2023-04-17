// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/i_metadata_packet.h>
#include <nx/sdk/interface.h>

namespace nx::sdk::analytics {

/**
 * Arbitrary metadata represented as a byte array.
 */
class ICustomMetadataPacket: public Interface<ICustomMetadataPacket, IMetadataPacket0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::ICustomMetadataPacket"); }

    /**
     * @return Null-terminated ASCII string describing the metadata format in a proprietary way.
     *     Never null, nor empty.
     */
    virtual const char* codec() const = 0;

    /**
     * @return Pointer to the compressed data. Is null if data size is zero.
     */
    virtual const char* data() const = 0;

    /**
     * @return Size of the compressed data in bytes.
     */
    virtual int dataSize() const = 0;

    /**
     * @return Pointer to the binary data specific to the particular metadata format, or null if
     *     such data is not available.
     */
    virtual const char* contextData() const = 0;

    /**
     * @return Size of the data returned by the contextData(), in bytes.
     */
    virtual int contextDataSize() const = 0;
};
using ICustomMetadataPacket0 = ICustomMetadataPacket;

} // namespace nx::sdk::analytics
