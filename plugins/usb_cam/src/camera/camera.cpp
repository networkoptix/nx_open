#include "camera.h"

#include <algorithm>

#include <nx/utils/log/log.h>

#include "../device/utils.h"
#include "utils.h"

namespace nx {
namespace usb_cam {

namespace {

size_t getPriorityCodecIndex(
    const std::vector<nxcip::CompressionType>& videoCodecPriorityList,
    const std::vector<device::CompressionTypeDescriptorPtr>& codecDescriptorList)
{
    for (const auto codecID : videoCodecPriorityList)
    {
        size_t index = 0;
        for (const auto& descriptor : codecDescriptorList)
        {
            if (codecID == descriptor->toNxCompressionType())
                return index;
            ++index;
        }
    }
    return codecDescriptorList.size();
}

device::CompressionTypeDescriptorPtr getPriorityDescriptor(
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


const std::vector<nxcip::CompressionType> Camera::kVideoCodecPriorityList = {
    nxcip::AV_CODEC_ID_H264,
    nxcip::AV_CODEC_ID_H263,
    nxcip::AV_CODEC_ID_MJPEG
};

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
    m_compressionTypeDescriptor = getPriorityDescriptor(kVideoCodecPriorityList, codecList);
    m_defaultVideoParams = getDefaultVideoParameters(url(), m_compressionTypeDescriptor);

    std::string video = std::string("VIDEO url: ") + m_info.url;
    std::string audio = std::string("AUDIO url: ") + m_info.auxiliaryData;
    std::string both = video + "\n" + audio;
    std::cout << both << std::endl;
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
            m_defaultVideoParams,
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

const device::CompressionTypeDescriptorPtr& Camera::compressionTypeDescriptor() const
{
    return m_compressionTypeDescriptor;
}

std::string Camera::url() const
{
    return utils::decodeCameraInfoUrl(m_info.url);
}

CodecParameters Camera::defaultVideoParameters() const
{
    return m_defaultVideoParams;
}

std::vector<AVCodecID> Camera::ffmpegCodecPriorityList()
{
    std::vector<AVCodecID> ffmpegCodecList;
    for (const auto & nxCodecID : kVideoCodecPriorityList)
        ffmpegCodecList.push_back(ffmpeg::utils::toAVCodecID(nxCodecID));
    return ffmpegCodecList;
}

CodecParameters Camera::getDefaultVideoParameters(
    const std::string& url,
    const device::CompressionTypeDescriptorPtr& descriptor)
{
    if (!descriptor)
        return CodecParameters(AV_CODEC_ID_NONE, 30, 5000000, 640, 640*9/16);

    nxcip::CompressionType nxCodecID = m_compressionTypeDescriptor->toNxCompressionType();
    AVCodecID ffmpegCodecID = ffmpeg::utils::toAVCodecID(nxCodecID);

    auto resolutionList = device::getResolutionList(url.c_str(), descriptor);
    auto it = std::max_element(resolutionList.begin(), resolutionList.end(),
        [](const device::ResolutionData& a, const device::ResolutionData& b)
    {
        return a.width * a.height < b.width * b.height;
    });

    if (it != resolutionList.end())
    {
        int maxBitrate = device::getMaxBitrate(url.c_str(), m_compressionTypeDescriptor);
        return CodecParameters(
            ffmpegCodecID,
            it->fps,
            maxBitrate,
            it->width,
            it->height);
    }
    else
        return CodecParameters(AV_CODEC_ID_NONE, 30, 5000000, 640, 640*9/16);
}

} // namespace usb_cam
} // namespace nx