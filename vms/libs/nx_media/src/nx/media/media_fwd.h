// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

namespace nx {

struct AudioFrame;

using AudioFramePtr = std::shared_ptr<AudioFrame>;
using ConstAudioFramePtr = std::shared_ptr<const AudioFrame>;

namespace media {

class VideoFrame;
using VideoFramePtr = std::shared_ptr<VideoFrame>;
using ConstVideoFramePtr = std::shared_ptr<const VideoFrame>;
struct AbstractRenderContextSynchronizer;
using RenderContextSynchronizerPtr = std::shared_ptr<AbstractRenderContextSynchronizer>;

// Media data alignment. We use 32 for compatibility with AVX instruction set.
static const int kMediaAlignment = 32;

// Initial duration for media buffer.
static const int kInitialBufferMs = 256;

// Maximum duration for live media buffer.
// Live buffer can be extended dynamically in range [kInitialBufferMs..kMaxLiveBufferMs]
static const int kMaxLiveBufferMs = 1200;

// Maximum duration for media buffer in archive mode.
static const int kMaxArchiveBufferMs = 3000;

// Increase buffer at N times if overflow/underflow issues.
static const double kBufferGrowStep = 2.0;

static const double kSpeedStep = 0.25;

} // namespace media
} // namespace nx
