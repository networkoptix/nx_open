// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/i_list.h>

#include <nx/sdk/analytics/i_timestamped_object_metadata.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>
#include <nx/sdk/analytics/i_compressed_video_packet.h>

namespace nx {
namespace sdk {
namespace analytics {

class IObjectTrackInfo: public Interface<IObjectTrackInfo>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IObjectTrackInfo"); }

    /** Called by track() */
    protected: virtual IList<ITimestampedObjectMetadata>* getTrack() const = 0;
    /** @return List of metadata that share the same trackId. */
    public: Ptr<IList<ITimestampedObjectMetadata>> track() const { return toPtr(getTrack()); }

    /** Called by bestShotVideoFrame() */
    protected: virtual IUncompressedVideoFrame* getBestShotVideoFrame() const = 0;
    /**
     * @return Frame defined to be the best shot in the object track by the DeviceAgent that
     *     generated this track, or, if such information is not available, the first frame of the
     *     object track. Returns null if the required frame is not available, or obtaining such a
     *     frame was not requested by the Engine manifest.
     */
    public: Ptr<IUncompressedVideoFrame> bestShotVideoFrame() const
    {
        return toPtr(getBestShotVideoFrame());
    }

    /** Called by bestShotObjectMetadata() */
    protected: virtual ITimestampedObjectMetadata* getBestShotObjectMetadata() const = 0;
    /**
     * @return Metadata for the position in the object track defined to be the best shot by the
     *     DeviceAgent that generated this track, or, if such information is not available, the
     *     first metadata of the object track. Returns null if the required metadata is not
     *     available, or obtaining such a metadata was not requested by the Engine manifest.
     */
    public: Ptr<ITimestampedObjectMetadata> bestShotObjectMetadata() const
    {
        return toPtr(getBestShotObjectMetadata());
    }
};

} // namespace analytics
} // namespace sdk
} // namespace nx
