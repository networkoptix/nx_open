#include "camera.h"

#include <algorithm>

#include <nx/utils/log/log.h>

#include "device/video/utils.h"

namespace nx::usb_cam {

Camera::Camera(
    const std::string& url,
    const nxcip::CameraInfo& cameraInfo,
    nxpl::TimeProvider* const timeProvider)
    :
    m_cameraInfo(cameraInfo),
    m_timeProvider(timeProvider),
    m_packetBuffer(toString())
{
    m_videoStream = std::make_unique<VideoStream>(url, m_timeProvider);
    m_audioStream = std::make_unique<AudioStream>(m_cameraInfo.auxiliaryData, m_timeProvider);
}

void Camera::setCredentials(const char* username, const char* password)
{
    strncpy(m_cameraInfo.defaultLogin, username, sizeof(m_cameraInfo.defaultLogin) - 1);
    strncpy(m_cameraInfo.defaultPassword, password, sizeof(m_cameraInfo.defaultPassword) - 1);
}

bool Camera::initialize()
{
    int status = m_videoStream->initialize();
    if (status < 0)
    {
        NX_DEBUG(this,
            "Failed to obtain a valid compression type descriptor error: %2, for camera: %1",
            status,
            toString());
        return false;
    }
    return true;
}

void Camera::uninitialize()
{
    NX_DEBUG(this, "unititialize");
    std::scoped_lock<std::mutex> lock(m_mutex);
    m_keyFrameFound = false;
    m_videoStream->uninitializeInput();
    m_audioStream->uninitialize();
}

int Camera::nextPacket(std::shared_ptr<ffmpeg::Packet>& packet)
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    int status = m_videoStream->nextPacket(packet);
    if (status < 0 && status != AVERROR(EAGAIN))
        return status;

    if (status == 0)
    {
        if (packet->mediaType() == AVMEDIA_TYPE_VIDEO && !m_keyFrameFound)
        {
            if (!packet->keyPacket())
            {
                packet.reset();
                m_lastError = AVERROR(EAGAIN);
                return AVERROR(EAGAIN);
            }
            m_keyFrameFound = true;
        }
        packet->setTimestamp(millisSinceEpoch());
    }
    else if (status == AVERROR(EAGAIN))
    {
        packet.reset();
        if (m_audioEnabled)
            status = m_audioStream->nextPacket(packet);
    }
    if (packet && m_enablePacketBuffering)
        m_packetBuffer.pushPacket(packet);

    m_lastError = status;
    return status;
}

int Camera::nextBufferedPacket(std::shared_ptr<ffmpeg::Packet>& packet)
{
    packet = m_packetBuffer.pop();
    if (packet)
        return 0;
    return AVERROR(EAGAIN);
}

void Camera::enablePacketBuffering(bool value)
{
    m_enablePacketBuffering = value;
    if (!value)
        m_packetBuffer.flush();
}

void Camera::interruptBufferPacketWait()
{
    m_packetBuffer.interrupt();
}

VideoStream& Camera::videoStream()
{
    return *m_videoStream;
}

AudioStream& Camera::audioStream()
{
    return *m_audioStream;
}

bool Camera::hasAudio() const
{
    return strlen(m_cameraInfo.auxiliaryData) != 0;
}

void Camera::setAudioEnabled(bool value)
{
    m_audioEnabled = value;
    if (!m_audioEnabled)
    {
        m_audioStream->uninitialize();
    }
}

bool Camera::audioEnabled() const
{
    return m_audioEnabled;
}

int Camera::lastError() const
{
    return m_lastError;
}

uint64_t Camera::millisSinceEpoch() const
{
    return m_timeProvider->millisSinceEpoch();
}

const nxcip::CameraInfo& Camera::info() const
{
    return m_cameraInfo;
}

std::string Camera::toString() const
{
    static const std::string prefix = "{ ";
    static const std::string suffix = " }";

    return prefix
        + "name: " + m_cameraInfo.modelName
        + ", uid: " + m_cameraInfo.uid
        + suffix;
}

} // namespace nx::usb_cam
