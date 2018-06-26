#include "transcode_media_encoder.h"

#include <nx/utils/log/log.h>

#include "camera_manager.h"
#include "transcode_stream_reader.h"
#include "utils/utils.h"
#include "ffmpeg/stream_reader.h"

namespace nx {
namespace webcam_plugin {

TranscodeMediaEncoder::TranscodeMediaEncoder(
    CameraManager* const cameraManager, 
    nxpl::TimeProvider *const timeProvider,
    const CodecContext& codecContext,
    const std::weak_ptr<ffmpeg::StreamReader>& ffmpegStreamReader)
:
    MediaEncoder(
        cameraManager,
        timeProvider,
        codecContext,
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
            &m_refManager,
            m_timeProvider,
            m_cameraManager->info(),
            m_videoCodecContext,
            m_ffmpegStreamReader));
    }

    m_streamReader->addRef();
    return m_streamReader.get();
}

} // namespace nx
} // namespace webcam_plugin