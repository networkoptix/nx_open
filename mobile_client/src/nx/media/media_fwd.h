#pragma once

class QVideoFrame;
typedef QSharedPointer<QVideoFrame> QnVideoFramePtr;
typedef QSharedPointer<const QVideoFrame> QnConstVideoFramePtr;

namespace nx {
namespace media {

class AbstractResourceAllocator;
class AudioFrame;

static const int kMediaAlignment = 32;

}
}

typedef QSharedPointer<nx::media::AudioFrame> QnAudioFramePtr;