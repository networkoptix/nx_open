// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/ref_countable.h>

#include <nx/sdk/analytics/i_object_track_info.h>

namespace nx {
namespace sdk {
namespace analytics {

class ObjectTrackInfo: public RefCountable<IObjectTrackInfo>
{
public:
    virtual IList<ITimestampedObjectMetadata>* track() const override;

    virtual IUncompressedVideoFrame* bestShotVideoFrame() const override;

    virtual ITimestampedObjectMetadata* bestShotObjectMetadata() const override;

    void setTrack(IList<ITimestampedObjectMetadata>* track);

    void setBestShotVideoFrame(IUncompressedVideoFrame* bestShotVideoFrame);

    void setBestShotObjectMetadata(ITimestampedObjectMetadata* bestShotObjectMetadata);

private:
    nx::sdk::Ptr<IList<ITimestampedObjectMetadata>> m_track;
    nx::sdk::Ptr<IUncompressedVideoFrame> m_bestShotVideoFrame;
    nx::sdk::Ptr<ITimestampedObjectMetadata> m_bestShotObjectMetadata;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
