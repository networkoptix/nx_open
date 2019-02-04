#include "transcode_stream_reader.h"

#include <set>

namespace nx::usb_cam {

TranscodeStreamReader::TranscodeStreamReader(
    int encoderIndex, const std::shared_ptr<Camera>& camera)
    :
    StreamReaderPrivate(encoderIndex, camera)
{
    m_codecParams = camera->defaultVideoParameters(); //< TODO make default secondary prarms
    m_transcoder.setTimeGetter([camera]() -> int64_t { return camera->millisSinceEpoch(); } );
}

TranscodeStreamReader::~TranscodeStreamReader()
{
    m_transcoder.uninitialize();
}

int TranscodeStreamReader::initializeTranscoder()
{
    AVCodecParameters* decoderCodecPar = m_camera->videoStream()->getCodecParameters();
    if (!decoderCodecPar)
        return AVERROR_DECODER_NOT_FOUND; //< Using as stream not found.

    return m_transcoder.initialize(decoderCodecPar, m_codecParams);
}

int TranscodeStreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    *lpPacket = nullptr;

    ensureConsumerAdded();

    auto packet = nextPacket();
    if (!packet)
        return handleNxError();

    *lpPacket = toNxPacket(packet.get()).release();

    return nxcip::NX_NO_ERROR;
}

void TranscodeStreamReader::setFps(float fps)
{
    if (m_codecParams.fps != fps)
    {
        m_codecParams.fps = fps;
        m_encoderNeedsReinitialization = true;
    }
}

void TranscodeStreamReader::setResolution(const nxcip::Resolution& resolution)
{
    if (m_codecParams.resolution.width != resolution.width ||
        m_codecParams.resolution.height != resolution.height)
    {
        m_codecParams.resolution = resolution;
        m_encoderNeedsReinitialization = true;
    }
}

void TranscodeStreamReader::setBitrate(int bitrate)
{
    if (m_codecParams.bitrate != bitrate)
    {
        m_codecParams.bitrate = bitrate;
        m_encoderNeedsReinitialization = true;
    }
}

std::shared_ptr<ffmpeg::Packet> TranscodeStreamReader::nextPacket()
{
    while (!shouldStop())
    {
        auto popped =  m_avConsumer->pop();
        if (!popped)
            continue;

        if (popped->mediaType() == AVMEDIA_TYPE_AUDIO)
            return popped;

        if (m_encoderNeedsReinitialization)
        {
            if (initializeTranscoder() < 0)
                return nullptr;

            m_encoderNeedsReinitialization = false;
        }

        std::shared_ptr<ffmpeg::Packet> encoded;
        int status = m_transcoder.transcode(popped.get(), encoded);
        if (status < 0)
        {
            m_camera->setLastError(status);
            return nullptr;
        }

        if (!encoded)
            continue;

        return encoded;
    }
    return nullptr;
}

} // namespace usb_cam::nx
