// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "frame_metadata.h"

#include <nx/media/video_frame.h>

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
    if (frame->context)
        codecName = frame->context->codecName();
}

FrameMetadata::FrameMetadata(const QnEmptyMediaDataPtr& frame):
    FrameMetadata()
{
    flags = frame->flags;
    sequence = frame->opaque;
    dataType = frame->dataType;
}

void FrameMetadata::serialize(const VideoFramePtr& frame) const
{
    if (!isNull())
        frame->metaData[kMetadataFlagsKey] = QVariant::fromValue(*this);
}

FrameMetadata FrameMetadata::deserialize(const ConstVideoFramePtr& frame)
{
    if (!frame)
        return FrameMetadata();

    auto data = frame->metaData.value(kMetadataFlagsKey);
    return data.canConvert<FrameMetadata>() ? data.value<FrameMetadata>() : FrameMetadata();
}

bool FrameMetadata::isNull() const
{
    return dataType == QnAbstractMediaData::UNKNOWN;
}

} // namespace media
} // namespace nx
