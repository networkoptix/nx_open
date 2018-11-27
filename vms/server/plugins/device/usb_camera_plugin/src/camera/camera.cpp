#include "camera.h"

#include <algorithm>

#include "camera_manager.h"
#include "device/video/utils.h"

namespace nx {
namespace usb_cam {

namespace {

static size_t getPriorityCodecIndex(
    const std::vector<nxcip::CompressionType>& videoCodecPriorityList,
    const std::vector<device::CompressionTypeDescriptorPtr>& codecDescriptorList)
{
    for (const auto codecId : videoCodecPriorityList)
    {
        size_t index = 0;
        for (const auto& descriptor : codecDescriptorList)
        {
            if (codecId == descriptor->toNxCompressionType())
                return index;
            ++index;
        }
    }
    return codecDescriptorList.size();
}

static device::CompressionTypeDescriptorPtr getPriorityDescriptor(
    const std::vector<nxcip::CompressionType>& videoCodecPriorityList,
    const std::vector<device::CompressionTypeDescriptorPtr>& codecDescriptorList)
{
    size_t index = getPriorityCodecIndex(videoCodecPriorityList, codecDescriptorList);
    if (index < codecDescriptorList.size())
        return codecDescriptorList[index];
    else if (codecDescriptorList.size() > 0)
        return codecDescriptorList[0];
    return nullptr;
}

} // namespace 

//--------------------------------------------------------------------------------------------------
// Camera

const std::vector<nxcip::CompressionType> Camera::kVideoCodecPriorityList = 
{
    nxcip::AV_CODEC_ID_H263,
    nxcip::AV_CODEC_ID_H264,
    nxcip::AV_CODEC_ID_MJPEG
};

Camera::Camera(
    CameraManager * cameraManager,
    nxpl::TimeProvider* const timeProvider):
    m_cameraManager(cameraManager),
    m_timeProvider(timeProvider),
    m_audioEnabled(false),
    m_lastError(0)
{
    auto codecList = device::video::getSupportedCodecs(url().c_str());
    m_compressionTypeDescriptor = getPriorityDescriptor(kVideoCodecPriorityList, codecList);

    // If m_compressionTypeDescriptor is null, there probably is no camera plugged in.
    if(m_compressionTypeDescriptor)
        m_defaultVideoParams = getDefaultVideoParameters();
    else
        setLastError(AVERROR(ENODEV));
}

void Camera::initialize()
{
    m_audioStream = std::make_shared<AudioStream>(
            info().auxiliaryData,
            weak_from_this(),
            m_audioEnabled);

    m_videoStream = std::make_shared<VideoStream>(
            weak_from_this(),
            m_defaultVideoParams);
}

std::shared_ptr<AudioStream> Camera::audioStream()
{
    return m_audioStream;
}

std::shared_ptr<VideoStream> Camera::videoStream()
{
    return m_videoStream;
}

std::vector<device::video::ResolutionData> Camera::resolutionList() const
{
    return device::video::getResolutionList(url(), m_compressionTypeDescriptor);
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

bool Camera::ioError() const
{
    return m_videoStream->ioError() || m_audioStream->ioError();
}

void Camera::setLastError(int errorCode)
{
    m_lastError = errorCode;
}

int Camera::lastError() const
{
    return m_lastError;
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

const device::CompressionTypeDescriptorPtr& Camera::compressionTypeDescriptor() const
{
    return m_compressionTypeDescriptor;
}

std::string Camera::url() const
{
    return m_cameraManager->getFfmpegUrl();
}

CodecParameters Camera::defaultVideoParameters() const
{
    return m_defaultVideoParams;
}

std::vector<AVCodecID> Camera::ffmpegCodecPriorityList()
{
    std::vector<AVCodecID> ffmpegCodecList;
    for (const auto & nxCodecID : kVideoCodecPriorityList)
        ffmpegCodecList.push_back(ffmpeg::utils::toAVCodecId(nxCodecID));
    return ffmpegCodecList;
}

nxcip::CameraInfo Camera::info() const
{
    return m_cameraManager->info();
}

CodecParameters Camera::getDefaultVideoParameters()
{
    nxcip::CompressionType nxCodecID = m_compressionTypeDescriptor->toNxCompressionType();
    AVCodecID ffmpegCodecID = ffmpeg::utils::toAVCodecId(nxCodecID);

    auto resolutionList = this->resolutionList();
    auto it = std::max_element(resolutionList.begin(), resolutionList.end(),
        [](const device::video::ResolutionData& a, const device::video::ResolutionData& b)
        {
            return a.width * a.height < b.width * b.height;
        });

    if (it != resolutionList.end())
    {
        int maxBitrate = device::video::getMaxBitrate(url().c_str(), m_compressionTypeDescriptor);
        return CodecParameters(
            ffmpegCodecID,
            it->fps,
            maxBitrate,
            nxcip::Resolution(it->width, it->height));
    }

    // Should never reach here if m_compressionTypeDescriptor is valid
    return CodecParameters();
}

} // namespace usb_cam
} // namespace nx
