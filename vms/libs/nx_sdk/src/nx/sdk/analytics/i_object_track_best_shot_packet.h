// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/i_metadata_packet.h>
#include <nx/sdk/analytics/rect.h>
#include <nx/sdk/i_attribute.h>
#include <nx/sdk/interface.h>

namespace nx::sdk::analytics {

/**
 * Packet containing information about object track best shot.
 */
class IObjectTrackBestShotPacket0: public Interface<IObjectTrackBestShotPacket0, IMetadataPacket0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IObjectTrackBestShotPacket"); }

    /**
     * @return Timestamp of the frame (in microseconds) the best shot belongs to, or, if such
     *     timestamp is not available, some timestamp close as much as possible to the best shot
     *     moment. Must be a positive value.
     */
    virtual int64_t timestampUs() const override = 0;

    /** Called by trackId() */
    protected: virtual void getTrackId(Uuid* outValue) const = 0;
    /** @return Id of the track the best shot belongs to. */
    public: Uuid trackId() const { Uuid value; getTrackId(&value); return value; }

    /** Called by boundingBox() */
    protected: virtual void getBoundingBox(Rect* outValue) const = 0;
    /**
     * @return Bounding box of the best shot, or an invalid rectangle (e.g. a default-constructed)
     *     if the best shot bounding box is unknown.
     */
    public: Rect boundingBox() const { Rect value; getBoundingBox(&value); return value; }
};

class IObjectTrackBestShotPacket1:
    public Interface<IObjectTrackBestShotPacket1, IObjectTrackBestShotPacket0>
{
public:
    static auto interfaceId()
    {
        return makeId("nx::sdk::analytics::IObjectTrackBestShotPacket1");
    }

    /**
     * @return HTTP or HTTPS URL of the image that should be used as the track best shot. Only
     *     JPEG, PNG and TIFF images are supported.
     */
    virtual const char* imageUrl() const = 0;

    /**
     * @return Pointer to the track best shot image data. Should return null if image URL is
     *     provided.
     */
    virtual const char* imageData() const = 0;

    /**
     * @return Size of the image data array in bytes.
     */
    virtual int imageDataSize() const = 0;

    /**
     * @return Format of the best shot image which is provided via imageData(). Can contain one of
     *     the following values: "image/jpeg", "image/png", "image/tiff" for JPEG, PNG and TIFF
     *     images correspondingly. If no image data is provided should return null.
     */
    virtual const char* imageDataFormat() const = 0;

    /** Called by attribute() */
    protected: virtual const IAttribute* getAttribute(int index) const = 0;
    /**
     * Provides values of so-called Metadata Attributes - typically, some object or event
     * properties (e.g. age or color), represented as a name-value map.
     * @param index 0-based index of the attribute.
     * @return Item of an attribute array, or null if index is out of range.
     */
    public: Ptr<const IAttribute> attribute(int index) const { return Ptr(getAttribute(index)); }

    /**
     * @return Number of items in the attribute array.
     */
    virtual int attributeCount() const = 0;
};

class IObjectTrackBestShotPacket:
    public Interface<IObjectTrackBestShotPacket, IObjectTrackBestShotPacket1>
{
public:
    static auto interfaceId()
    {
        return makeId("nx::sdk::analytics::IObjectTrackBestShotPacket2");
    }

    virtual Flags flags() const = 0;
};
using IObjectTrackBestShotPacket2 = IObjectTrackBestShotPacket;

} // namespace nx::sdk::analytics
