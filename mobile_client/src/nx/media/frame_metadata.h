#pragma once

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>

#include <nx/streaming/video_data_packet.h>

#include "media_fwd.h"

namespace nx {
namespace media {

/* 
* This structure contains addition information associated with every decoded frame 
*/
struct FrameMetadata
{
    FrameMetadata();
    FrameMetadata(const QnConstCompressedVideoDataPtr& frame);
    void serialize(const QnVideoFramePtr& frame) const;
    static FrameMetadata deserialize(const QnConstVideoFramePtr& frame);
            
    QnAbstractMediaData::MediaFlags flags; //< various flags passed from compressed video data
    bool noDelay; //< display frame immediately with no delay
    int frameNum; //< frame number in range [0..int_max]
};

}
}

Q_DECLARE_METATYPE(nx::media::FrameMetadata);
