#include "camera.h"

#include <algorithm>

#include <nx/utils/log/log.h>

#include "discovery_manager.h"
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
    nxcip::AV_CODEC_ID_H263
    ,nxcip::AV_CODEC_ID_H264
    ,nxcip::AV_CODEC_ID_MJPEG
};

Camera::Camera(
    DiscoveryManager * discoveryManager,
    const nxcip::CameraInfo& cameraInfo,
    nxpl::TimeProvider* const timeProvider):
    m_discoveryManager(discoveryManager),
    m_cameraInfo(cameraInfo),
    m_timeProvider(timeProvider)
{
}

void Camera::setCredentials(const char * username, const char * password)
{
    strncpy( m_cameraInfo.defaultLogin, username, sizeof(m_cameraInfo.defaultLogin)-1 );
    strncpy( m_cameraInfo.defaultPassword, password, sizeof(m_cameraInfo.defaultPassword)-1 );
}

bool Camera::initialize()
{
    if (isInitialized())
        return true;

    auto codecList = device::video::getSupportedCodecs(ffmpegUrl().c_str());
    m_compressionTypeDescriptor = getPriorityDescriptor(kVideoCodecPriorityList, codecList);

    // If m_compressionTypeDescriptor is null, there probably is no camera plugged in.
    if(!m_compressionTypeDescriptor)
    {
        NX_DEBUG(
            this,
            "%1: Failed to obtain a valid compression type descriptor for camera: %2",
            __func__,
            toString());

        setLastError(AVERROR(ENODEV));
        return false;
    }

    m_defaultVideoParams = getDefaultVideoParameters();

    m_videoStream = std::make_shared<VideoStream>(
        weak_from_this(),
        m_defaultVideoParams);

    m_audioStream = std::make_shared<AudioStream>(
        info().auxiliaryData,
        weak_from_this(),
        m_audioEnabled);

    return isInitialized();
}

bool Camera::isInitialized() const
{
    return m_compressionTypeDescriptor && m_videoStream && m_audioStream;
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
    return device::video::getResolutionList(ffmpegUrl(), m_compressionTypeDescriptor);
}

bool Camera::hasAudio() const
{
    return strlen(m_cameraInfo.auxiliaryData) != 0;
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

CodecParameters Camera::defaultVideoParameters() const
{
    return m_defaultVideoParams;
}

std::string Camera::ffmpegUrl() const
{
    //return m_cameraManager->ffmpegUrl();
    return m_discoveryManager->getFfmpegUrl(m_cameraInfo.uid);
}

std::vector<AVCodecID> Camera::ffmpegCodecPriorityList()
{
    std::vector<AVCodecID> ffmpegCodecList;
    for (const auto & nxCodecID : kVideoCodecPriorityList)
        ffmpegCodecList.push_back(ffmpeg::utils::toAVCodecId(nxCodecID));
    return ffmpegCodecList;
}

const nxcip::CameraInfo& Camera::info() const
{
    //return m_cameraManager->info();
    return m_cameraInfo;
}

std::string Camera::toString() const
{
    static const std::string prefix = "{ ";
    static const std::string suffix = " }";

    return
        prefix 
        + "name: " + m_cameraInfo.modelName 
        + ", uid: " + m_cameraInfo.uid 
        + ", video:" + ffmpegUrl()
        + ", audio: " + m_cameraInfo.auxiliaryData 
        + suffix;
}

CodecParameters Camera::getDefaultVideoParameters()
{
    if(!m_compressionTypeDescriptor)
        return CodecParameters();

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
        int maxBitrate = 
            device::video::getMaxBitrate(ffmpegUrl().c_str(), m_compressionTypeDescriptor);
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
