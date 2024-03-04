// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/i_compressed_video_packet.h>
#include <nx/sdk/analytics/i_timestamped_object_metadata.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>
#include <nx/sdk/i_list.h>
#include <nx/sdk/interface.h>

namespace nx::sdk::analytics {

class IObjectTrackInfo0: public Interface<IObjectTrackInfo0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IObjectTrackInfo"); }

    /** Called by track() */
    protected: virtual IList<ITimestampedObjectMetadata>* getTrack() const = 0;
    /** @return List of metadata that share the same trackId. */
    public: Ptr<IList<ITimestampedObjectMetadata>> track() const { return Ptr(getTrack()); }

    /** Called by bestShotVideoFrame() */
    protected: virtual IUncompressedVideoFrame* getBestShotVideoFrame() const = 0;
    /**
     * @return Frame defined to be the Best Shot in the Object Track by the DeviceAgent that
     *     generated this track, or, if such information is not available, the first frame of the
     *     object track. Returns null if the required frame is not available, or obtaining such a
     *     frame was not requested by the Engine manifest.
     */
    public: Ptr<IUncompressedVideoFrame> bestShotVideoFrame() const
    {
        return Ptr(getBestShotVideoFrame());
    }

    /** Called by bestShotObjectMetadata() */
    protected: virtual ITimestampedObjectMetadata* getBestShotObjectMetadata() const = 0;
    /**
     * @return Metadata for the position in the object track defined to be the Best Shot by the
     *     DeviceAgent that generated this track, or, if such information is not available, the
     *     first metadata of the object track. Returns null if the required metadata is not
     *     available, or obtaining such a metadata was not requested by the Engine manifest.
     */
    public: Ptr<ITimestampedObjectMetadata> bestShotObjectMetadata() const
    {
        return Ptr(getBestShotObjectMetadata());
    }
};

class IObjectTrackInfo1: public Interface<IObjectTrackInfo1, IObjectTrackInfo0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IObjectTrackInfo1"); }

    /**
     * @return Pointer to the Track Best Shot image data provided by the Plugin. If the Best
     *     Shot was provided in the form of a URL, the pointer being returned by this method points
     *     to the data of the image that server obtained by this URL. If the Plugin provides
     *     neither binary image data nor its URL, null is returned.
     */
    virtual const char* bestShotImageData() const = 0;

    /**
     * @return Size of the image data array. If the Track has no explicit Best Shot image, 0 is
     *     returned.
     */
    virtual int bestShotImageDataSize() const = 0;

    /**
     * @return Format of the Best Shot image. Can contain one of the following values:
     *     "image/jpeg", "image/png", "image/tiff". If the track has no explicit Best Shot image,
     *     an empty string is returned.
     */
    virtual const char* bestShotImageDataFormat() const = 0;
};

class IObjectTrackInfo: public Interface<IObjectTrackInfo, IObjectTrackInfo1>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IObjectTrackInfo2"); }

    /**
     * @return Title text, or null if the Track does not have a title.
     */
    virtual const char* titleText() const = 0;

    /**
     * @return Pointer to the Track Title image data provided by the Plugin. If the Title was
     *     provided in the form of a URL, the pointer being returned by this method points
     *     to the data of the image that the Server obtained by this URL. If the Plugin provides
     *     neither binary image data nor its URL, null is returned.
     */
    virtual const char* titleImageData() const = 0;

    /**
     * @return Size of the image data array. If the track has no explicit Title image, 0 is
     *     returned.
     */
    virtual int titleImageDataSize() const = 0;

    /**
     * @return Format of the Title image. Can contain one of the following values:
     *     "image/jpeg", "image/png", "image/tiff". If the track has no explicit Title image,
     *     an empty string is returned.
     */
    virtual const char* titleImageDataFormat() const = 0;
};
using IObjectTrackInfo2 = IObjectTrackInfo;

} // namespace nx::sdk::analytics
