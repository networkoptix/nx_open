// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/streaming/audio_data_packet.h>
#include "audio_decoder_registry.h"
#include "abstract_audio_decoder.h"

#include "media_fwd.h"

namespace nx {
namespace media {

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
    /**
     * Decode an audio frame. This is a sync function and it could take a lot of CPU. This call is
     * not thread-safe.
     * @param frame Compressed audio data. If frame is null, then the function must flush internal
     * decoder buffer. If no more frames in the buffer are left, return true as result and null
     * shared pointer in the 'result' parameter.
     * @return True if the frame is decoded without errors. For null input data, return true while
     * flushing internal buffer (result is not null).
     */
    bool decode(const QnConstCompressedAudioDataPtr& frame, double speed);

    /** @return result Decoded audio data. If the decoder still fills internal buffer, then result
     * can be empty but the function returns true.
     */
    AudioFramePtr nextFrame();

private:
    AudioDecoderPtr m_audioDecoder;
    AVCodecID m_currentCodecId = AV_CODEC_ID_NONE;
    double m_currentSpeed = 0;
};

} // namespace media
} // namespace nx
