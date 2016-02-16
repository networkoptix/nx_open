#pragma once

#include <QtCore/QObject>

#include <nx/streaming/audio_data_packet.h>

#include "media_fwd.h"

namespace nx {
namespace media {

class SeamlessAudioDecoderPrivate;

/**
 * This class encapsulates common logic related to any audio decoder. It guarantees seamless
 * decoding in case a compressed frame has changed codecId. AudioDecoder uses 
 * PhysicalDecoderFactory to instantiate compatible PhysicalDecoder to decode next frame if audio
 * parameters have changed.
 */
class SeamlessAudioDecoder: public QObject
{
    Q_OBJECT

public:
    SeamlessAudioDecoder();
    virtual ~SeamlessAudioDecoder();

    /**
     * Decode an audio frame. This is a sync function and it could take a lot of CPU. This call is
     * not thread-safe.
     * @param frame Compressed audio data. If frame is null, then the function must flush internal
     * decoder buffer. If no more frames in the buffer are left, return true as result and null
     * shared pointer in the 'result' parameter.
     * @param result Decoded audio data. If the decoder still fills internal buffer, then result
     * can be empty but the function returns true.
     * @return True if the frame is decoded without errors. For null input data, return true while
     * flushing internal buffer (result is not null).
     */
    bool decode(const QnConstCompressedAudioDataPtr& frame, AudioFramePtr* result = nullptr);

    /**
     * @return Current frame number in range [0..INT_MAX]. This number will be used for the next
     * frame on decode() call.
     */
    int currentFrameNumber() const;

    void pleaseStop();

private:
    QScopedPointer<SeamlessAudioDecoderPrivate> d_ptr;
    Q_DECLARE_PRIVATE(SeamlessAudioDecoder);
};

} // namespace media
} // namespace nx
