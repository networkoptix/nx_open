#include "native_media_encoder.h"

#include <nx/utils/log/log.h>

#include "native_stream_reader.h"
#include "camera_manager.h"
#include "stream_reader.h"

namespace nx {
namespace rpi_cam2 {

NativeMediaEncoder::NativeMediaEncoder(
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

NativeMediaEncoder::~NativeMediaEncoder()
{
}

nxcip::StreamReader* NativeMediaEncoder::getLiveStreamReader()
{
    if (!m_streamReader)
    {
        m_streamReader.reset(new NativeStreamReader(
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