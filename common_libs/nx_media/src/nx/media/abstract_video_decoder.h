#pragma once

#include <QtCore/QObject>
#include <QtMultimedia/QVideoFrame>
#include <QtGui/QOpenGLContext>

#include <nx/streaming/video_data_packet.h>

#include "media_fwd.h"

namespace nx {
namespace media {

/*
* Interface for video decoder implementation.
*/
class AbstractVideoDecoder: public QObject
{
    Q_OBJECT
public:
    AbstractVideoDecoder() {}
    virtual ~AbstractVideoDecoder() {}
    virtual void setAllocator(AbstractResourceAllocator* /*allocator */) {}

    /*
    * \param context    codec context.
    * \returns true if decoder is compatible with provided context.
    * This function should be overridden despite static keyword. Otherwise it'll be compile error.
    */
    static bool isCompatible(const CodecID /*codec */, const QSize& /*resolution*/) { return false; }

    /*
    * Decode a video frame. This is a sync function and it could take a lot of CPU.
    * It's guarantee all source frames have same codec context.
    *
    * \param frame        compressed video data. If frame is null pointer then function must flush internal decoder buffer.
    *  If no more frames are left in the buffer, return zero value as a result, and return a null shared pointer in the "result" parameter.
    * \param result        decoded video data. If decoder still fills internal buffer then result can be empty but function returns 0.
    * \!returns decoded frame number (value >=0)  if frame is decoded without errors. Returns negative value if decoding error.
    * For nullptr input data returns positive value while decoder is flushing internal buffer (result isn't null).
    */
    virtual int decode(const QnConstCompressedVideoDataPtr& frame, QVideoFramePtr* result = nullptr) = 0;
};

}
}
