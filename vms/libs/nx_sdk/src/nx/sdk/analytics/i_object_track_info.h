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
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::IObjectTrackInfo"); }

    /** @return List of metadata for all objects sharing the same objectId. */
    virtual IList<ITimestampedObjectMetadata>* track() const = 0;

    /**
     * @return Frame defined to be the best shot in the object track by the DeviceAgent that
     *     generated this track, or, if such information is not available, the first frame of the
     *     object track. Returns null if the required frame is not available, or obtaining such a
     *     frame was not requested by the Engine manifest.
     */
    virtual IUncompressedVideoFrame* bestShotVideoFrame() const = 0;

    /**
     * @return Metadata for the position in the object track defined to be the best shot by the
     *     DeviceAgent that generated this track, or, if such information is not available, the
     *     first metadata of the object track. Returns null if the required metadata is not
     *     available, or obtaining such a metadata was not requested by the Engine manifest.
     */
    virtual ITimestampedObjectMetadata* bestShotObjectMetadata() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
