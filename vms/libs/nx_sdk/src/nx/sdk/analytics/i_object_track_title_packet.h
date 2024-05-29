// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/i_metadata_packet.h>
#include <nx/sdk/analytics/rect.h>
#include <nx/sdk/interface.h>

namespace nx::sdk::analytics {

/**
 * Packet containing information about the optional Title of an Object Track.
 */
class IObjectTrackTitlePacket: public Interface<IObjectTrackTitlePacket, IMetadataPacket0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IObjectTrackTitlePacket"); }

    /**
     * @return Timestamp of the frame (in microseconds) the Title Image and Text are associated
     *     with, or some timestamp close as much as possible to this moment, or -1 if no such
     *     information is available.
     */
    virtual int64_t timestampUs() const override = 0;

    /** Called by trackId() */
    protected: virtual void getTrackId(Uuid* outValue) const = 0;
    /** @return Id of the Object Track the Title belongs to. */
    public: Uuid trackId() const { Uuid value; getTrackId(&value); return value; }

    /** Called by boundingBox() */
    protected: virtual void getBoundingBox(Rect* outValue) const = 0;
    /**
     * @return Bounding box of the Object Title, or an invalid rectangle (e.g. a default-constructed)
     *     if the Object Title bounding box is unknown.
     */
    public: Rect boundingBox() const { Rect value; getBoundingBox(&value); return value; }

    /**
     * @return Title text. Can be empty, but not null.
     */
    virtual const char* text() const = 0;

    /**
     * @return HTTP or HTTPS URL of the image that should be used as the Track Title. Only JPEG,
     *     PNG and TIFF images are supported.
     */
    virtual const char* imageUrl() const = 0;

    /**
     * @return Pointer to the Track Title image data. Must return null if an image URL is
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
     *     images correspondingly. If no image data is provided, must return null.
     */
    virtual const char* imageDataFormat() const = 0;

    virtual Flags flags() const = 0;
};
using IObjectTrackTitlePacket0 = IObjectTrackTitlePacket;

} // namespace nx::sdk::analytics
