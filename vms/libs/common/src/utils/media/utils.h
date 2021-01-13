#pragma once

#include <QtCore/QSize>
#include "nx/streaming/video_data_packet.h"

/** todo: move it to nx_media */

namespace nx::media {

QSize getFrameSize(const QnConstCompressedVideoDataPtr& frame);
double getDefaultSampleAspectRatio(const QSize& srcSize);
bool fillExtraData(const QnConstCompressedVideoDataPtr& video, AVCodecContext* context);

} // namespace nx::media

