#include "transcode_stream_reader.h"

namespace nx::usb_cam {

TranscodeStreamReader::TranscodeStreamReader(const std::shared_ptr<Camera>& camera):
    m_camera(camera)
{
    m_codecParams = camera->defaultVideoParameters(); //< TODO make default secondary params
    m_transcoder.setTimeGetter([camera]() -> int64_t { return camera->millisSinceEpoch(); } );
}

int TranscodeStreamReader::initializeTranscoder()
{
    AVCodecParameters* decoderCodecPar = m_camera->videoStream()->getCodecParameters();
    if (!decoderCodecPar)
        return AVERROR_DECODER_NOT_FOUND; //< Using as stream not found.

    return m_transcoder.initialize(decoderCodecPar, m_codecParams);
}

int TranscodeStreamReader::processPacket(
    const std::shared_ptr<ffmpeg::Packet>& source, std::shared_ptr<ffmpeg::Packet>& result)
{
    if (source->mediaType() == AVMEDIA_TYPE_AUDIO)
    {
        result = source;
        return 0;
    }

    int status = 0;
    if (m_encoderNeedsReinitialization)
    {
        status = initializeTranscoder();
        if (status < 0)
            return status;
        m_encoderNeedsReinitialization = false;
    }

    status = m_transcoder.transcode(source.get(), result);
    if (status < 0)
        return status;

    return status;
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

} // namespace usb_cam::nx
