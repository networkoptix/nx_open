#include "native_media_encoder.h"

#include <nx/utils/log/log.h>

#include "stream_reader.h"
#include "camera_manager.h"

namespace nx {
namespace usb_cam {

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
        m_streamReader.reset(new StreamReader(
            m_encoderIndex,
            m_timeProvider,
            m_codecParams,
            m_ffmpegStreamReader,
            &m_refManager));
    }

    m_streamReader->addRef();
    return m_streamReader.get();
}


} // namespace nx
} // namespace usb_cam