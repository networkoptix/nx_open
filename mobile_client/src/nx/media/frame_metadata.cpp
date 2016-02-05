#include "frame_metadata.h"

namespace
{
    static QString kMetadataFlagsKey(lit("metadata"));
    static const int kNotInitialized = -1; //< metadata isn't initialized
}

namespace nx {
namespace media {


FrameMetadata::FrameMetadata():
    flags(QnAbstractMediaData::MediaFlags_None),
    noDelay(false),
    frameNum(kNotInitialized)
{
}

FrameMetadata::FrameMetadata(const QnConstCompressedVideoDataPtr& frame):
    FrameMetadata()
{
    flags = frame->flags;
}

void FrameMetadata::serialize(const QnVideoFramePtr& frame) const
{
    if (!isNull())
        frame->setMetaData(kMetadataFlagsKey, QVariant::fromValue(*this));
}

FrameMetadata FrameMetadata::deserialize(const QSharedPointer<const QVideoFrame>& frame)
{
    if (!frame)
        return FrameMetadata();

    auto data = frame->metaData(kMetadataFlagsKey);
    return data.canConvert<FrameMetadata>() ? data.value<FrameMetadata>() : FrameMetadata();
}

bool FrameMetadata::isNull() const 
{ 
    return frameNum == kNotInitialized; 
}

} // namespace media
} // namespace nx
