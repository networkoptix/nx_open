#include "frame_metadata.h"

namespace nx {
namespace media {

namespace {

static const QString kMetadataFlagsKey("metadata");

} // namespace
FrameMetadata::FrameMetadata():
    flags(QnAbstractMediaData::MediaFlags_None),
    displayHint(DisplayHint::regular),
    frameNum(-1),
    sar(1.0),
    videoChannel(0),
    sequence(0),
    dataType(QnAbstractMediaData::UNKNOWN)
{
}

FrameMetadata::FrameMetadata(const QnConstCompressedVideoDataPtr& frame):
    FrameMetadata()
{
    videoChannel = frame->channelNumber;
    flags = frame->flags;
    sequence = frame->opaque;
    dataType = frame->dataType;
}

FrameMetadata::FrameMetadata(const QnEmptyMediaDataPtr& frame):
    FrameMetadata()
{
    flags = frame->flags;
    sequence = frame->opaque;
    dataType = frame->dataType;
}

void FrameMetadata::serialize(const QVideoFramePtr& frame) const
{
    if (!isNull())
        frame->setMetaData(kMetadataFlagsKey, QVariant::fromValue(*this));
}

FrameMetadata FrameMetadata::deserialize(const QnConstVideoFramePtr& frame)
{
    if (!frame)
        return FrameMetadata();

    auto data = frame->metaData(kMetadataFlagsKey);
    return data.canConvert<FrameMetadata>() ? data.value<FrameMetadata>() : FrameMetadata();
}

bool FrameMetadata::isNull() const
{
    return dataType == QnAbstractMediaData::UNKNOWN;
}

} // namespace media
} // namespace nx
