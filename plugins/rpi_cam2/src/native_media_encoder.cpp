#include "native_media_encoder.h"

#include <nx/utils/log/log.h>

#include "camera_manager.h"
#include "stream_reader.h"
#include "utils/utils.h"

namespace nx {
namespace rpi_cam2 {

NativeMediaEncoder::NativeMediaEncoder(
    CameraManager* const cameraManager,
    nxpl::TimeProvider *const timeProvider,
    const CodecContext& codecContext,
    const std::shared_ptr<nx::ffmpeg::StreamReader>& ffmpegStreamReader)
:
MediaEncoder(
    cameraManager,
    timeProvider,
    codecContext,
    ffmpegStreamReader)
{
}

NativeMediaEncoder::~NativeMediaEncoder()
{
}

nxcip::StreamReader* NativeMediaEncoder::getLiveStreamReader()
{
    if (!m_streamReader)
    {
        m_streamReader.reset(new NativeStreamReader(
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
} // namespace rpi_cam2