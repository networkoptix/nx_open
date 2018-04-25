#pragma once

#include <memory>

class QVideoFrame;

namespace nx {

struct AudioFrame;

using QVideoFramePtr = std::shared_ptr<QVideoFrame>;
using QnConstVideoFramePtr = std::shared_ptr<const QVideoFrame>;

using AudioFramePtr = std::shared_ptr<AudioFrame>;
using ConstAudioFramePtr = std::shared_ptr<const AudioFrame>;

namespace media {

struct AbstractRenderContextSynchronizer;
using RenderContextSynchronizerPtr = std::shared_ptr<AbstractRenderContextSynchronizer>;

static const int kMediaAlignment = 32;

// Initial duration for media buffer.
static const int kInitialBufferMs = 256;

// Maximum duration for live media buffer.
// Live buffer can be extended dynamically in range [kInitialBufferMs..kMaxLiveBufferMs]
static const int kMaxLiveBufferMs = 1200;

// Maximum duration for media buffer in archive mode.
static const int kMaxArchiveBufferMs = 3000;

// Increase buffer at N times if overflow/underflow issues.
static const qreal kBufferGrowStep = 2.0;

} // namespace media
} // namespace nx
