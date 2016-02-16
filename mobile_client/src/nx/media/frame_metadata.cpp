#include "frame_metadata.h"

namespace nx {
namespace media {

namespace {

static const QString kMetadataFlagsKey(lit("metadata"));

static const int kNotInitialized = -1; //< Metadata is not initialized.

} // namespace

FrameMetadata::FrameMetadata()
:
    flags(QnAbstractMediaData::MediaFlags_None),
    noDelay(false),
    frameNum(kNotInitialized)
{
}

FrameMetadata::FrameMetadata(const QnConstCompressedVideoDataPtr& frame)
:
    FrameMetadata()
{
    flags = frame->flags;
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
    return frameNum == kNotInitialized;
}

} // namespace media
} // namespace nx
