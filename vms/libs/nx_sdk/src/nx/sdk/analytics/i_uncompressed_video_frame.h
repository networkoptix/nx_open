// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include <nx/sdk/i_list.h>
#include <nx/sdk/analytics/i_metadata_packet.h>
#include <nx/sdk/analytics/i_uncompressed_media_frame.h>

namespace nx {
namespace sdk {
namespace analytics {

class IUncompressedVideoFrame0: public Interface<IUncompressedVideoFrame0, IUncompressedMediaFrame>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IUncompressedVideoFrame"); }

    enum class PixelFormat: int
    {
        yuv420,
        argb,
        abgr,
        rgba,
        bgra,
        rgb,
        bgr,
        count
    };

    struct PixelAspectRatio
    {
        int numerator;
        int denominator;
    };

    /**
     * @return width of the decoded frame in pixels.
     */
    virtual int width() const = 0;

    /**
     * @return Height of the decoded frame in pixels.
     */
    virtual int height() const = 0;

    /**
     * @return Aspect ratio of a frame pixel.
     */
    protected: virtual void getPixelAspectRatio(PixelAspectRatio* outValue) const = 0;
    public: PixelAspectRatio pixelAspectRatio() const
    {
        PixelAspectRatio value;
        getPixelAspectRatio(&value);
        return value;
    }

    virtual PixelFormat pixelFormat() const = 0;

    /**
     * @param plane Number of the plane, in range 0..planeCount().
     * @return Number of bytes in each pixel line of the plane, or 0 if the data is not accessible.
     */
    virtual int lineSize(int plane) const = 0;
};

/**
 * Represents a single uncompressed video frame.
 */
class IUncompressedVideoFrame: public Interface<IUncompressedVideoFrame, IUncompressedVideoFrame0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IUncompressedVideoFrame0"); }

    protected: virtual IList<IMetadataPacket>* getMetadataList() const = 0;
    public: Ptr<IList<IMetadataPacket>> metadataList() const
    {
        return toPtr(getMetadataList());
    }
};

} // namespace analytics
} // namespace sdk
} // namespace nx
