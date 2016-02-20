#pragma once

class QVideoFrame;

namespace nx {

class AudioFrame;

typedef std::shared_ptr<QVideoFrame> QVideoFramePtr;
typedef std::shared_ptr<const QVideoFrame> QnConstVideoFramePtr;

typedef std::shared_ptr<AudioFrame> AudioFramePtr;
typedef std::shared_ptr<const AudioFrame> ConstAudioFramePtr;

namespace media {

class AbstractResourceAllocator;
struct AudioFrame;

static const int kMediaAlignment = 32;

// Initial duration for media buffer.
static const int kInitialBufferMs = 200;

// Maximum duration for live media buffer. 
// Live buffer can be extended dynamically in range [kInitialBufferMs..kMaxLiveBufferMs]
static const int kMaxLiveBufferMs = 1200;

// Maximum duration for media buffer in archive mode.
static const int kMaxArchiveBufferMs = 3000;

// Increase buffer at N times if overflow/underflow issues.
static const qreal kBufferGrowStep = 2.0;

} // namespace media
} // namespace nx
