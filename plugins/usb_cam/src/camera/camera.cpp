#include "camera.h"

#include <algorithm>

#include <nx/utils/log/log.h>

#include "../device/utils.h"
#include "utils.h"

namespace nx {
namespace usb_cam {

Camera::Camera(
    nxpl::TimeProvider* const timeProvider,
    const nxcip::CameraInfo& info)
    :
    m_timeProvider(timeProvider),
    m_info(info),
    m_audioEnabled(false),
    m_lastError(0)
{
    auto codecList = device::getSupportedCodecs(url().c_str());
    m_compressionTypeDescriptor = utils::getPriorityDescriptor(codecList);
    assignDefaultParams(m_compressionTypeDescriptor);
}

std::shared_ptr<AudioStream> Camera::audioStream()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_audioStream)
    {
        m_audioStream = std::make_shared<AudioStream>(
            m_info.auxiliaryData,
            weak_from_this(),
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
            url(),
            m_videoCodecParams,
            weak_from_this());
    }
    return m_videoStream;
}

std::vector<device::ResolutionData> Camera::getResolutionList() const
{
    return device::getResolutionList(url().c_str(), m_compressionTypeDescriptor);
}

void Camera::setAudioEnabled(bool value)
{
    m_audioEnabled = value;
    if(m_audioStream)
        m_audioStream->setEnabled(value);
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

uint64_t Camera::millisSinceEpoch() const
{
    return m_timeProvider->millisSinceEpoch();
}

nxpl::TimeProvider * const Camera::timeProvider() const
{
    m_timeProvider->addRef();
    return m_timeProvider;
}

device::CompressionTypeDescriptorConstPtr Camera::compressionTypeDescriptor() const
{
    return std::const_pointer_cast<const device::AbstractCompressionTypeDescriptor>(
        m_compressionTypeDescriptor);
}

std::string Camera::url() const
{
    return utils::decodeCameraInfoUrl(m_info.url);
}

CodecParameters Camera::codecParameters() const
{
    return m_videoCodecParams;
}

void Camera::assignDefaultParams(const device::CompressionTypeDescriptorPtr& descriptor)
{
    std::string url = this->url();
    NX_ASSERT(descriptor);
    if (!descriptor)
    {
        m_videoCodecParams = CodecParameters(AV_CODEC_ID_NONE, 30, 5000000, 640, 640*9/16);
        return;
    }

    nxcip::CompressionType nxCodecID = m_compressionTypeDescriptor->toNxCompressionType();
    AVCodecID ffmpegCodecID = ffmpeg::utils::toAVCodecID(nxCodecID);

    auto resolutionList = device::getResolutionList(url.c_str(), m_compressionTypeDescriptor);
    auto it = std::max_element(resolutionList.begin(), resolutionList.end(),
        [](const device::ResolutionData& a, const device::ResolutionData& b)
    {
        return a.width * a.height < b.width * b.height;
    });

    if (it != resolutionList.end())
    {
        int maxBitrate = device::getMaxBitrate(url.c_str(), nxCodecID);
        m_videoCodecParams =  CodecParameters(
            ffmpegCodecID,
            it->fps,
            maxBitrate,
            it->width,
            it->height);
    }
    else
    {
        NX_ASSERT(false);
        m_videoCodecParams = CodecParameters(AV_CODEC_ID_NONE, 30, 5000000, 640, 640*9/16);
    }
}

} // namespace usb_cam
} // namespace nx