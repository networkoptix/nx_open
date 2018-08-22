#include "camera.h"

#include <iostream>

#include "utils.h"

namespace nx {
namespace usb_cam {

Camera::Camera(
    nxpl::TimeProvider* const timeProvider,
    const nxcip::CameraInfo& info,
    const ffmpeg::CodecParameters& codecParams)
    :
    m_timeProvider(timeProvider),
    m_info(info),
    m_videoCodecParams(codecParams),
    m_audioEnabled(false),
    m_lastError(0)
{   
}

std::shared_ptr<ffmpeg::AudioStreamReader> Camera::audioStreamReader()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_audioStreamReader)
    {
        m_audioStreamReader = std::make_shared<ffmpeg::AudioStreamReader>(
            m_info.auxiliaryData,
            m_timeProvider,
            m_audioEnabled);
    }
    return m_audioStreamReader;
}
    
std::shared_ptr<ffmpeg::VideoStreamReader> Camera::videoStreamReader()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_videoStreamReader)
    {
        m_videoStreamReader = std::make_shared<ffmpeg::VideoStreamReader>(
            utils::decodeCameraInfoUrl(m_info.url),
            m_videoCodecParams,
            m_timeProvider);
    }
    return m_videoStreamReader;
}

void Camera::setAudioEnabled(bool value)
{
    if(m_audioEnabled != value)
    {
        m_audioEnabled = value;
        if(m_audioStreamReader)
            m_audioStreamReader->setEnabled(value);
    }
}

bool Camera::audioEnabled() const
{
    return m_audioEnabled;
}

void Camera::setLastError(int errorCode)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastError = errorCode;
}

int Camera::lastError() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

const nxcip::CameraInfo& Camera::info() const
{
    return m_info;
}

void Camera::updateCameraInfo(const nxcip::CameraInfo& info)
{
    m_info = info;
}

int64_t Camera::millisSinceEpoch() const
{
    return m_timeProvider->millisSinceEpoch();
}

} // namespace usb_cam
} // namespace nx