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
    void setTrack(IList<ITimestampedObjectMetadata>* track);
    void setBestShotVideoFrame(IUncompressedVideoFrame* bestShotVideoFrame);
    void setBestShotObjectMetadata(ITimestampedObjectMetadata* bestShotObjectMetadata);

protected:
    virtual IList<ITimestampedObjectMetadata>* getTrack() const override;
    virtual IUncompressedVideoFrame* getBestShotVideoFrame() const override;
    virtual ITimestampedObjectMetadata* getBestShotObjectMetadata() const override;

private:
    Ptr<IList<ITimestampedObjectMetadata>> m_track;
    Ptr<IUncompressedVideoFrame> m_bestShotVideoFrame;
    Ptr<ITimestampedObjectMetadata> m_bestShotObjectMetadata;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
