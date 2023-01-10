// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once


#include <QtMultimedia/QVideoFrame>

#include <nx/reflect/instrument.h>
#include <nx/streaming/video_data_packet.h>

#include "media_fwd.h"

namespace nx {
namespace media {

NX_REFLECTION_ENUM_CLASS(DisplayHint,
    regular, //< Regular frame.
    obsolete, //< Frame is obsolete because of seek or it's coarse frame.
    firstRegular //< First regular frame. No need to sleep before display it.
)

/**
 * Contains addition information associated with every decoded frame.
 */
struct FrameMetadata
{
    FrameMetadata();
    FrameMetadata(const QnConstCompressedVideoDataPtr& frame);
    FrameMetadata(const QnEmptyMediaDataPtr& frame);

    void serialize(const QVideoFramePtr& frame) const;
    static FrameMetadata deserialize(const QnConstVideoFramePtr& frame);

    QnAbstractMediaData::MediaFlags flags; /**< Various flags passed from compressed video data. */
    DisplayHint displayHint; /**< Display frame immediately with no delay. */
    int frameNum; /**< Frame number in range [0..INT_MAX]. */
    double sar; /**< Sample(pixel) aspect ratio. */
    int videoChannel; /**< For multi-sensor cameras. */
    int sequence; /**< Number of playback request. */
    QnAbstractMediaData::DataType dataType;
private:
    bool isNull() const;
};

#define FrameMetadata_Fields (flags)(displayHint)(frameNum)(sar)(videoChannel)(sequence)(dataType)
NX_REFLECTION_INSTRUMENT(FrameMetadata, FrameMetadata_Fields)

} // namespace media
} // namespace nx
