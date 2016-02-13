#pragma once

class QVideoFrame;
typedef QSharedPointer<QVideoFrame> QnVideoFramePtr;
typedef QSharedPointer<const QVideoFrame> QnConstVideoFramePtr;

namespace nx {
namespace media {

class AbstractResourceAllocator;
class AudioFrame;

static const int kMediaAlignment = 32;
static const int kInitialBufferMs = 200;   //< initial duration for media buffer
static const int kMaxBufferMs = 1200;      //< maximum duration for media buffer. Buffer can be extended dynamically
static const qreal kBufferGrowStep = 2.0;  //< increase buffer at N times if overflow/underflow issues

}
}

typedef QSharedPointer<nx::media::AudioFrame> QnAudioFramePtr;