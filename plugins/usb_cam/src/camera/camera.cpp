#include "camera.h"

#include <iostream>

#include "utils.h"

namespace nx {
namespace usb_cam {

Camera::Camera(
    nxpl::TimeProvider* const timeProvider,
    const nxcip::CameraInfo& info,
    const CodecParameters& codecParams)
    :
    m_timeProvider(timeProvider),
    m_info(info),
    m_videoCodecParams(codecParams),
    m_audioEnabled(false),
    m_lastError(0)
{   
}

std::shared_ptr<AudioStream> Camera::audioStream()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_audioStream)
    {
        m_audioStream = std::make_shared<AudioStream>(
            m_info.auxiliaryData,
            m_timeProvider,
            m_audioEnabled);
    }
    return m_audioStream;
}
    
std::shared_ptr<VideoStream> Camera::videoStream()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_videoStream)
    {
        m_videoStream = std::make_shared<VideoStream>(
            utils::decodeCameraInfoUrl(m_info.url),
            m_videoCodecParams,
            m_timeProvider);
    }
    return m_videoStream;
}

void Camera::setAudioEnabled(bool value)
{
    if(m_audioEnabled != value)
    {
        m_audioEnabled = value;
        if(m_audioStream)
            m_audioStream->setEnabled(value);
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