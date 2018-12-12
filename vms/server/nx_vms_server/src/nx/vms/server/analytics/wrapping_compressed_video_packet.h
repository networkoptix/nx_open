#pragma once

#include <nx/streaming/video_data_packet.h>

#include "generic_compressed_video_packet.h"

namespace nx {
namespace vms::server {
namespace analytics {

/**
 * Wrapper owning a QnCompressedVideoData.
 */
class WrappingCompressedVideoPacket: public GenericCompressedVideoPacket
{
public:
    WrappingCompressedVideoPacket(const QnConstCompressedVideoDataPtr& frame):
        m_frame(frame)
    {
        setTimestampUs(m_frame->timestamp);
        setWidth(m_frame->width);
        setHeight(m_frame->height);
        setCodec(toString(m_frame->compressionType).toStdString());

        if (m_frame->flags & QnAbstractMediaData::MediaFlag::MediaFlags_AVKey)
            addFlag(MediaFlag::keyFrame);

        setExternalData(m_frame->data(), (int) m_frame->dataSize());
    }

private:
    QnConstCompressedVideoDataPtr m_frame;
};

} // namespace analytics
} // namespace vms::server
} // namespace nx
