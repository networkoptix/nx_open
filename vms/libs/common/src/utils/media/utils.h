#pragma once

#include <QtCore/QSize>
#include "nx/streaming/video_data_packet.h"

/** todo: move it to nx_media */

namespace nx {
namespace media {

QSize getFrameSize(const QnConstCompressedVideoDataPtr& frame);
double getDefaultSampleAspectRatio(const QSize& srcSize);

} // namespace media
} // namespace nx
