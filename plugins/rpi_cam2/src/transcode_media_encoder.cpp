#include "transcode_media_encoder.h"

#include "camera_manager.h"
#include "transcode_stream_reader.h"
#include "ffmpeg/stream_reader.h"

namespace nx {
namespace rpi_cam2 {

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
        m_streamReader.reset(new TranscodeStreamReader(
            m_encoderIndex,
            &m_refManager,
            m_timeProvider,
            m_cameraManager->info(),
            m_codecParams,
            m_ffmpegStreamReader));
    }

    m_streamReader->addRef();
    return m_streamReader.get();
}

} // namespace nx
} // namespace rpi_cam2