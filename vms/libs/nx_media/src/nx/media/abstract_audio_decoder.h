#pragma once

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>
#include <QtGui/QOpenGLContext>

#include <nx/streaming/audio_data_packet.h>
#include <utils/common/byte_array.h>

#include "media_fwd.h"

namespace nx {

struct AudioFrame
{
    AudioFrame();

    QnByteArray data;               //< decoded audio data
    qint64 timestampUsec;           //< UTC timestamp
    QnConstMediaContextPtr context; //< codec context. decoder must keep context from input data
};

namespace media {

/*
* Interface for video decoder implementation.
*/
class AbstractAudioDecoder: public QObject
{
    Q_OBJECT
public:
    AbstractAudioDecoder() {}
    virtual ~AbstractAudioDecoder() {}

    /*
    * \param context    codec context.
    * \returns true if decoder is compatible with provided context.
    * This function should be overridden despite static keyword. Otherwise it'll be compile error.
    */
    static bool isCompatible(const AVCodecID /*codec */) { return false; }

    /*
    * Decode a audio frame. This is a sync function and it could take a lot of CPU.
    * It's guarantee all source frames have same codec context.
    *
    * \param frame        compressed audio data. 
    * \param outFrame     decoded audio data.
    * \!returns true if no decoder error. outFrame may stay empty if decoder fills internal buffer.
    */
    virtual bool decode(const QnConstCompressedAudioDataPtr& frame, AudioFramePtr* const outFrame) = 0;
};

}
}
