// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "seamless_audio_decoder.h"

namespace nx {
namespace media {

bool SeamlessAudioDecoder::decode(const QnConstCompressedAudioDataPtr& frame, double speed)
{
    if (m_currentCodecId != frame->compressionType ||
        fabs(m_currentSpeed - speed) >= kSpeedStep)
    {
        // Release previous decoder in case the hardware decoder can handle only single instance.
        m_audioDecoder.reset();

        m_audioDecoder = AudioDecoderRegistry::instance()->createCompatibleDecoder(
            frame->compressionType, speed);

        if (!m_audioDecoder)
            return false;

        m_currentSpeed = speed;
        m_currentCodecId = frame->compressionType;
    }
    m_audioDecoder->decode(frame, speed);
    return true;
}

AudioFramePtr SeamlessAudioDecoder::nextFrame()
{
    if (!m_audioDecoder)
        return nullptr;

    return m_audioDecoder->nextFrame();
}

} // namespace media
} // namespace nx
