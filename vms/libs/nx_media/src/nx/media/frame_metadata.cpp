// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "frame_metadata.h"

#include <nx/reflect/json.h>
#include <nx/utils/serialization/flags.h>

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
        frame->setSubtitleText(QString::fromStdString(nx::reflect::json::serialize(*this)));
}

FrameMetadata FrameMetadata::deserialize(const QnConstVideoFramePtr& frame)
{
    if (!frame)
        return FrameMetadata();

    FrameMetadata metadata;
    return nx::reflect::json::deserialize(frame->subtitleText().toStdString(), &metadata)
        ? metadata
        : FrameMetadata();
}

bool FrameMetadata::isNull() const
{
    return dataType == QnAbstractMediaData::UNKNOWN;
}

} // namespace media
} // namespace nx
