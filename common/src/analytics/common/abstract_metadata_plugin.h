#pragma once

#include <utils/media/frame_info.h>
#include <nx/streaming/abstract_data_packet.h>
#include <nx/streaming/media_data_packet.h>
#include <analytics/common/abstract_metadata.h>
#include <nx/streaming/video_data_packet.h>

namespace nx {
namespace analytics {

class AbstractMetadataPlugin
{

public:
    virtual QString id() const = 0;
    virtual bool hasMetadata() = 0;
    virtual QnAbstractCompressedMetadataPtr getNextMetadata() = 0;
    virtual void reset() {}
};

class VideoMetadataPlugin: public AbstractMetadataPlugin
{
public:
    virtual bool pushFrame(const CLVideoDecoderOutputPtr& decodedFrame) = 0;
    virtual bool pushFrame(const QnCompressedVideoDataPtr& compressedFrame) = 0;

    // TODO: #dmishin it's a temporary hack, remove it.
    virtual void setChannelNumber(int i) { m_channel = i; }

protected:
    int m_channel;
};

} // namespace analytics
} // namespace nx
