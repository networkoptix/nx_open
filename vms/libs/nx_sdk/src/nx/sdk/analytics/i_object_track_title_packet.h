// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/i_metadata_packet.h>
#include <nx/sdk/interface.h>

namespace nx::sdk::analytics {

/**
 * Packet containing information about the optional Title of an Object Track.
 */
class IObjectTrackTitlePacket: public Interface<IObjectTrackTitlePacket, IMetadataPacket0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IObjectTrackTitlePacket"); }

    /** Called by trackId() */
    protected: virtual void getTrackId(Uuid* outValue) const = 0;
    /** @return Id of the Object Track the Title belongs to. */
    public: Uuid trackId() const { Uuid value; getTrackId(&value); return value; }

    /**
     * @return Title text.
     */
    virtual const char* text() const = 0;

    /**
     * @return HTTP or HTTPS URL of the image that should be used as the Track Title. Only JPEG,
     *     PNG and TIFF images are supported.
     */
    virtual const char* imageUrl() const = 0;

    /**
     * @return Pointer to the Track Title image data. Should return null if the image URL is
     *     provided.
     */
    virtual const char* imageData() const = 0;

    /**
     * @return Size of the image data array in bytes.
     */
    virtual int imageDataSize() const = 0;

    /**
     * @return Format of the Title image which is provided via imageData(). Can contain one of the
     *     following values: "image/jpeg", "image/png", "image/tiff" for JPEG, PNG and TIFF
     *     images correspondingly. If no image data is provided, should return null.
     */
    virtual const char* imageDataFormat() const = 0;
};
using IObjectTrackTitlePacket0 = IObjectTrackTitlePacket;

} // namespace nx::sdk::analytics
