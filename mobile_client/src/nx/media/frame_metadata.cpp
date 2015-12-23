#include "frame_metadata.h"

namespace
{
    static QString kMetadataFlagsKey(lit("metadata"));
}

namespace nx
{
namespace media
{


FrameMetadata::FrameMetadata():
    flags(QnAbstractMediaData::MediaFlags_None)
{
}

FrameMetadata::FrameMetadata(const QnConstCompressedVideoDataPtr& frame):
    flags(frame->flags)
{
}

void FrameMetadata::serialize(const QSharedPointer<QVideoFrame>& frame) const
{
    frame->setMetaData(kMetadataFlagsKey, QVariant::fromValue(*this));
}

FrameMetadata FrameMetadata::deserialize(const QSharedPointer<const QVideoFrame>& frame)
{
    if (!frame)
        return FrameMetadata();

    auto data = frame->metaData(kMetadataFlagsKey);
    return data.canConvert<FrameMetadata>() ? data.value<FrameMetadata>() : FrameMetadata();
}


}
}
