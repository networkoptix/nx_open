#pragma once

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#include <nx/streaming/video_data_packet.h>

#include "media_fwd.h"

namespace nx {
namespace media {

/** 
 * Contains addition information associated with every decoded frame.
 */
struct FrameMetadata
{
    FrameMetadata();
    FrameMetadata(const QnConstCompressedVideoDataPtr& frame);

    bool isNull() const;
    void serialize(const QVideoFramePtr& frame) const;
    static FrameMetadata deserialize(const QnConstVideoFramePtr& frame);
            
    QnAbstractMediaData::MediaFlags flags; /**< Various flags passed from compressed video data. */
    bool noDelay; /**< Display frame immediately with no delay. */
    int frameNum; /**< Frame number in range [0..INT_MAX]. */
};

} // namespace media
} // namespace nx

Q_DECLARE_METATYPE(nx::media::FrameMetadata);
