// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/i_compressed_media_packet.h>
#include <nx/sdk/analytics/i_metadata_packet.h>
#include <nx/sdk/i_list.h>
#include <nx/sdk/interface.h>

namespace nx::sdk::analytics {

class ICompressedVideoPacket0: public Interface<ICompressedVideoPacket0, ICompressedMediaPacket0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::ICompressedVideoPacket"); }

    /**
     * @return Width of the video frame in pixels.
     */
    virtual int width() const = 0;

    /**
     * @return Height of the video frame in pixels.
     */
    virtual int height() const = 0;
};

/**
 * Represents a single video frame.
 */
class ICompressedVideoPacket: public Interface<ICompressedVideoPacket, ICompressedVideoPacket0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::ICompressedVideoPacket1"); }

    /** Called by metadataList() */
    protected: virtual IList<IMetadataPacket>* getMetadataList() const = 0;
    public: Ptr<IList<IMetadataPacket>> metadataList() const
    {
        return Ptr(getMetadataList());
    }
};
using ICompressedVideoPacket1 = ICompressedVideoPacket;

} // namespace nx::sdk::analytics
