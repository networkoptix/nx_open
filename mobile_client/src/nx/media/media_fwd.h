#pragma once

class QVideoFrame;
typedef QSharedPointer<QVideoFrame> QnVideoFramePtr;
typedef QSharedPointer<const QVideoFrame> QnConstVideoFramePtr;

namespace nx {
namespace media {

class AbstractResourceAllocator;
class AudioFrame;

static const int kMediaAlignment = 32;

// Initial duration for media buffer.
static const int kInitialBufferMs = 200;

// Maximum duration for media buffer. Buffer can be extended dynamically.
static const int kMaxBufferMs = 1200;

// Increase buffer at N times if overflow/underflow issues.
static const qreal kBufferGrowStep = 2.0;

} // namespace media
} // namespace nx

typedef QSharedPointer<nx::media::AudioFrame> QnAudioFramePtr;
