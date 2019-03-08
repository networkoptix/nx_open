#include "video_stream.h"

#include <algorithm>

#include <nx/utils/log/log.h>
#include <nx/utils/app_info.h>
#include <plugins/plugin_container_api.h>
#include <utils/media/frame_type_extractor.h>

#include "camera.h"
#include "device/video/utils.h"
#include "ffmpeg/utils.h"

namespace nx::usb_cam {

namespace {

static constexpr int kMsecInSec = 1000;

static const char* ffmpegDeviceTypePlatformDependent()
{
    return
#ifdef _WIN32
        "dshow";
#else
        "v4l2";
#endif
}

#ifdef _WIN32
static bool isKeyFrame(const ffmpeg::Packet* packet)
{
    if (!packet)
        return false;

    const quint8* udata = static_cast<const quint8*>(packet->data());

    FrameTypeExtractor extractor(packet->codecId());

    return extractor.getFrameType(udata, packet->size()) == FrameTypeExtractor::I_Frame;
}
#endif

static device::CompressionTypeDescriptorPtr getPriorityDescriptor(
    const std::vector<device::CompressionTypeDescriptorPtr>& codecDescriptorList)
{
    static const std::vector<nxcip::CompressionType> kVideoCodecPriorityList =
    {
        nxcip::AV_CODEC_ID_H263,
        nxcip::AV_CODEC_ID_H264,
        nxcip::AV_CODEC_ID_MJPEG
    };

    for (const auto codecId: kVideoCodecPriorityList)
    {
        for (const auto& descriptor: codecDescriptorList)
        {
            if (codecId == descriptor->toNxCompressionType())
                return descriptor;
        }
    }
    if (!codecDescriptorList.empty())
        return codecDescriptorList[0];

    return nullptr;
}

} // namespace

VideoStream::VideoStream(const std::string& url, nxpl::TimeProvider* timeProvider)
    :
    m_url(url),
    m_timeProvider(timeProvider)
{
}

VideoStream::~VideoStream()
{
    uninitializeInput();
}

bool VideoStream::isVideoCompressed()
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    return m_codecParams.codecId != AV_CODEC_ID_NONE;
}

void VideoStream::updateUrl(const std::string& url)
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    NX_DEBUG(this, "update url: %1", url);
    m_needReinitialization = true;
    m_url = url;
}

bool VideoStream::pluggedIn() const
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    return !device::video::getDeviceName(m_url.c_str()).empty();
}

std::string VideoStream::ffmpegUrlPlatformDependent() const
{
    return
#ifdef _WIN32
        std::string("video=@device_pnp_") + m_url;
#else
        m_url;
#endif
}

CodecParameters VideoStream::getDefaultVideoParameters()
{
    nxcip::CompressionType nxCodecID = m_compressionTypeDescriptor->toNxCompressionType();
    AVCodecID ffmpegCodecID = ffmpeg::utils::toAVCodecId(nxCodecID);

    auto resolutionList = device::video::getResolutionList(m_url, m_compressionTypeDescriptor);
    auto it = std::max_element(resolutionList.begin(), resolutionList.end(),
        [](const device::video::ResolutionData& a, const device::video::ResolutionData& b)
        {
            return a.height < b.height && a.fps <= b.fps;
        });

    if (it != resolutionList.end())
    {
        int maxBitrate =
            device::video::getMaxBitrate(m_url.c_str(), m_compressionTypeDescriptor);
        return CodecParameters(
            ffmpegCodecID,
            it->fps,
            maxBitrate,
            nxcip::Resolution(it->width, it->height));
    }

    // Should never reach here if m_compressionTypeDescriptor is valid
    return CodecParameters();
}

int VideoStream::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_compressionTypeDescriptor)
        return 0;

    auto codecList = device::video::getSupportedCodecs(m_url);
    m_compressionTypeDescriptor = getPriorityDescriptor(codecList);
    // If m_compressionTypeDescriptor is null, there probably is no camera plugged in.
    if (!m_compressionTypeDescriptor)
        return AVERROR(ENODEV);

    m_codecParams = getDefaultVideoParameters();
    return true;
}

void VideoStream::uninitializeInput()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_inputFormat.reset(nullptr);
    m_needReinitialization = true;
}

AVCodecParameters* VideoStream::getCodecParameters()
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    if (!m_inputFormat)
        return nullptr;
    AVStream* stream = m_inputFormat->findStream(AVMEDIA_TYPE_VIDEO);
    if (!stream)
        return nullptr;

    return stream->codecpar;
}

int VideoStream::getMaxBitrate()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return device::video::getMaxBitrate(m_url, m_compressionTypeDescriptor);
}

int VideoStream::initializeInput()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto inputFormat = std::make_unique<ffmpeg::InputFormat>();
    int result = inputFormat->initialize(ffmpegDeviceTypePlatformDependent());
    if (result < 0)
        return result;

    setInputFormatOptions(inputFormat);

    result = inputFormat->open(ffmpegUrlPlatformDependent().c_str());
    if (result < 0)
        return result;

    m_inputFormat = std::move(inputFormat);
    return 0;
}

void VideoStream::setInputFormatOptions(std::unique_ptr<ffmpeg::InputFormat>& inputFormat)
{
    NX_DEBUG(this, "Camera %1 attempting to open video stream with params: %2",
        m_url,
        m_codecParams.toString());

    AVFormatContext * context = inputFormat->formatContext();
    if (m_codecParams.codecId != AV_CODEC_ID_NONE)
        context->video_codec_id = m_codecParams.codecId;

    // Dshow input format drops frames if its real time buffer gets too full.
    // If the camera is streaming in H264 format, dropping will corrupt the stream.
    // So, two things may help: instructing the AVFormatContext not to buffer packets when it can
    // avoid it, and increasing the default real time buffer size from 3041280 (3M) to 20M.
    context->flags |= AVFMT_FLAG_NOBUFFER; //< Could be helpful for Linux too.
    context->flags |= AVFMT_FLAG_NONBLOCK;

    if (m_codecParams.fps > 0)
        inputFormat->setFps(m_codecParams.fps);

    if (m_codecParams.resolution.width * m_codecParams.resolution.height > 0)
        inputFormat->setResolution(m_codecParams.resolution.width, m_codecParams.resolution.height);

    // Note: setting the bitrate only works for raspberry pi mmal camera
    if (m_codecParams.bitrate > 0)
    {
        // ffmpeg doesn't have an option for setting the bitrate on AVFormatContext.
        device::video::setBitrate(
            m_url,
            m_codecParams.bitrate,
            m_compressionTypeDescriptor);
    }
}

int VideoStream::nextPacket(std::shared_ptr<ffmpeg::Packet>& result)
{
    if (!pluggedIn())
        return AVERROR(EIO);

    int status;
    if (m_needReinitialization)
    {
        uninitializeInput();
        status = initializeInput();
        if (status < 0)
            return status;
        m_needReinitialization = false;
    }
    auto packetTemp = std::make_shared<ffmpeg::Packet>(
        m_inputFormat->videoCodecId(), AVMEDIA_TYPE_VIDEO);

    status = m_inputFormat->readFrame(packetTemp->packet());
    if (status < 0)
        return status;

    result = std::make_shared<ffmpeg::Packet>(
        m_inputFormat->videoCodecId(), AVMEDIA_TYPE_VIDEO);
    status = result->copy(*(packetTemp->packet()));
    if (status < 0)
        return status;

#ifdef _WIN32
    // Dshow input format does not set h264 key packet flag correctly, so do it manually
    if (result->codecId() == AV_CODEC_ID_H264 && isKeyFrame(result.get()))
        result->packet()->flags |= AV_PKT_FLAG_KEY;
#endif

    // Setting timestamp here because primary stream needs it even if there is no decoding.
    result->setTimestamp(m_timeProvider->millisSinceEpoch());
    return status;
}

std::vector<device::video::ResolutionData> VideoStream::resolutionList() const
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    return device::video::getResolutionList(m_url, m_compressionTypeDescriptor);
}

CodecParameters VideoStream::codecParameters()
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    return m_codecParams;
}

void VideoStream::setFps(float fps)
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    if (fps == m_codecParams.fps)
        return;

    CodecParameters newParams = m_codecParams;
    newParams.fps = fps;
    CodecParameters finalParams = findClosestHardwareConfiguration(newParams);
    setCodecParameters(finalParams);
}

void VideoStream::setResolution(const nxcip::Resolution& resolution)
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    if (m_codecParams.resolution.width == resolution.width &&
        m_codecParams.resolution.height == resolution.height)
        return;

    CodecParameters newParams = m_codecParams;
    newParams.resolution = resolution;
    auto finalParams = findClosestHardwareConfiguration(newParams);
    setCodecParameters(finalParams);
}

void VideoStream::setBitrate(int bitrate)
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    if (m_codecParams.bitrate == bitrate)
        return;

    m_codecParams.bitrate = bitrate;
    m_needReinitialization = true;
}

CodecParameters VideoStream::findClosestHardwareConfiguration(const CodecParameters& params) const
{
    std::vector<device::video::ResolutionData> resolutionList;
    // Assumes list is in ascending resolution order
    resolutionList = device::video::getResolutionList(m_url, m_compressionTypeDescriptor);

    // Try to find an exact match first
    for (const auto & resolution : resolutionList)
    {
        if (resolution.width == params.resolution.width &&
            resolution.height == params.resolution.height &&
            resolution.fps == params.fps)
        {
            return CodecParameters(
                m_codecParams.codecId,
                params.fps,
                m_codecParams.bitrate,
                params.resolution);
        }
    }

    // Then a match with similar aspect ratio whose resolution and fps are higher than requested
    float aspectRatio = (float) params.resolution.width / params.resolution.height;
    for (const auto& resolution: resolutionList)
    {
        if (aspectRatio == resolution.aspectRatio())
        {
            bool actualResolutionGreater = resolution.width * resolution.height >=
                params.resolution.width * params.resolution.height;
            if (actualResolutionGreater && resolution.fps >= params.fps)
            {
                return CodecParameters(
                    m_codecParams.codecId,
                    resolution.fps,
                    m_codecParams.bitrate,
                    nxcip::Resolution(resolution.width, resolution.height));
            }
        }
    }

    // Any resolution or fps higher than requested
    for (const auto& resolution: resolutionList)
    {
        bool actualResolutionGreater = resolution.width * resolution.height >=
            params.resolution.width * params.resolution.height;
        if (actualResolutionGreater && resolution.fps >= params.fps)
        {
            return CodecParameters(
                m_codecParams.codecId,
                resolution.fps,
                m_codecParams.bitrate,
                nxcip::Resolution(resolution.width, resolution.height));
        }
    }

    return m_codecParams;
}

void VideoStream::setCodecParameters(const CodecParameters& codecParams)
{
    bool newResolutionNotEqual = m_codecParams.resolution.width * m_codecParams.resolution.height
        != codecParams.resolution.width * codecParams.resolution.height;

    if (m_codecParams.fps != codecParams.fps || newResolutionNotEqual)
    {
        AVCodecID codecId = m_codecParams.codecId;
        m_codecParams = codecParams;
        m_codecParams.codecId = codecId;
        m_needReinitialization = true;
    }
}

} // namespace nx::usb_cam
