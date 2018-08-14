#include "transcode_media_encoder.h"

#include "camera_manager.h"
#include "stream_reader.h"
#include "ffmpeg/stream_reader.h"
#include "transcode_stream_reader.h"

namespace nx {
namespace usb_cam {

TranscodeMediaEncoder::TranscodeMediaEncoder(
    int encoderIndex,
    const ffmpeg::CodecParameters& codecParams,
    CameraManager* const cameraManager, 
    nxpl::TimeProvider *const timeProvider,
    const std::shared_ptr<nx::ffmpeg::StreamReader>& ffmpegStreamReader)
:
    MediaEncoder(
        encoderIndex,
        codecParams,
        cameraManager,
        timeProvider,
        ffmpegStreamReader)
{
}

TranscodeMediaEncoder::~TranscodeMediaEncoder()
{
}

nxcip::StreamReader* TranscodeMediaEncoder::getLiveStreamReader()
{
    if (!m_streamReader)
    {
        std::unique_ptr<StreamReaderPrivate> transcoder = std::make_unique<TranscodeStreamReader>(
            m_encoderIndex,
            m_timeProvider,
            m_codecParams,
            m_ffmpegStreamReader);

        m_streamReader.reset(new StreamReader(
            m_timeProvider,
            &m_refManager,
            transcoder));
    }

    m_streamReader->addRef();
    return m_streamReader.get();
}

} // namespace nx
} // namespace usb_cam