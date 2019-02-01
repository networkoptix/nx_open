#include "video_stream.h"

#include <algorithm>

#include <nx/utils/log/log.h>
#include <nx/utils/app_info.h>
#include <plugins/plugin_container_api.h>
#include <utils/media/frame_type_extractor.h>

#include "camera.h"
#include "buffered_stream_consumer.h"
#include "device/video/utils.h"
#include "ffmpeg/utils.h"

namespace nx {
namespace usb_cam {

namespace {

static constexpr int kMsecInSec = 1000;

static const char * ffmpegDeviceTypePlatformDependent()
{
    return
#ifdef _WIN32
        "dshow";
#else
        "v4l2";
#endif
}

#ifdef _WIN32
static bool isKeyFrame(const ffmpeg::Packet * packet)
{
    if (!packet)
        return false;

    const quint8 * udata = static_cast<const quint8 *>(packet->data());

    FrameTypeExtractor extractor(packet->codecId());

    return extractor.getFrameType(udata, packet->size()) == FrameTypeExtractor::I_Frame;
}
#endif

} // namespace

VideoStream::VideoStream(
    const std::weak_ptr<Camera>& camera,
    const CodecParameters& codecParams)
    :
    m_camera(camera),
    m_codecParams(codecParams),
    m_timeProvider(camera.lock()->timeProvider())
{
}

VideoStream::~VideoStream()
{
    stop();
    m_timeProvider->releaseRef();
}

std::string VideoStream::ffmpegUrl() const
{
    if (auto cam = m_camera.lock())
        return cam->ffmpegUrl();
    return {};
}

float VideoStream::fps() const
{
    return m_codecParams.fps;
}

float VideoStream::actualFps() const
{
    return m_fpsCounter.actualFps > 0 ? m_fpsCounter.actualFps.load() : m_codecParams.fps;
}

std::chrono::milliseconds VideoStream::timePerFrame() const
{
    return std::chrono::milliseconds((int)(1.0f / fps() * kMsecInSec));
}

std::chrono::milliseconds VideoStream::actualTimePerFrame() const
{
    return std::chrono::milliseconds((int)(1.0f / actualFps() * kMsecInSec));
}

void VideoStream::addPacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_threadStartMutex);
    m_packetConsumerManager.addConsumer(consumer);
    tryToStartIfNotStarted();
}

void VideoStream::removePacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_threadStartMutex);
    m_packetConsumerManager.removeConsumer(consumer);

    if (m_packetConsumerManager.empty())
        stop();

    // Raspberry Pi: reinitialize the camera to recover frame rate on the primary stream.
    if (nx::utils::AppInfo::isRaspberryPi())
        setCameraState(csModified);
}

bool VideoStream::ioError() const
{
    return m_ioError;
}

bool VideoStream::pluggedIn() const
{
    return !device::video::getDeviceName(ffmpegUrl().c_str()).empty();
}

std::string VideoStream::ffmpegUrlPlatformDependent() const
{
    return
#ifdef _WIN32
        std::string("video=@device_pnp_") + ffmpegUrl();
#else
        ffmpegUrl();
#endif
}

void VideoStream::tryToStartIfNotStarted()
{
    if (m_terminated)
    {
        if (pluggedIn())
            start();
    }
}

void VideoStream::start()
{
    stop();

    m_terminated = false;
    m_videoThread = std::thread(&VideoStream::run, this);
}

void VideoStream::stop()
{
    if (!m_terminated)
    {
        m_terminated = true;
        if (m_videoThread.joinable())
            m_videoThread.join();
    }
}

void VideoStream::run()
{
    while (!m_terminated)
    {
        if (!ensureInitialized())
            continue;

        auto packet = readFrame();
        if (!packet)
            continue;

        m_fpsCounter.update(std::chrono::milliseconds(m_timeProvider->millisSinceEpoch()));
        m_packetConsumerManager.givePacket(packet);
    }
    uninitialize();
}

bool VideoStream::ensureInitialized()
{
    bool needInit;
    bool needUninit;
    {
        needUninit = m_streamState == csModified;
        needInit = needUninit || m_streamState == csOff;
    }

    if (needUninit)
        uninitialize();

    if (needInit)
    {
        m_initCode = initialize();
        checkIoError(m_initCode);
        if (m_initCode < 0)
        {
            setLastError(m_initCode);
            if (m_ioError)
                m_terminated = true;
        }
    }

    return m_streamState == csInitialized;
}

int VideoStream::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    int result = initializeInputFormat();
    if (result < 0)
        return result;

    setCameraState(csInitialized);
    return 0;
}

void VideoStream::uninitialize()
{
    // Some cameras segfault if they are unintialized while there are still packets, so need flush
    m_packetConsumerManager.flush();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_inputFormat.reset(nullptr);

        setCameraState(csOff);
    }
}

AVCodecParameters* VideoStream::getCodecParameters()
{
    if (!m_inputFormat)
        return nullptr;
    AVStream * stream = m_inputFormat->findStream(AVMEDIA_TYPE_VIDEO);
    if (!stream)
        return nullptr;

    return stream->codecpar;
}

int VideoStream::initializeInputFormat()
{
    auto inputFormat = std::make_unique<ffmpeg::InputFormat>();
    int result = inputFormat->initialize(ffmpegDeviceTypePlatformDependent());
    if (result < 0)
        return result;

    setInputFormatOptions(inputFormat);

    result = inputFormat->open(ffmpegUrlPlatformDependent().c_str());
    checkIoError(result);
    if (result < 0)
        return result;

    m_inputFormat = std::move(inputFormat);

    return 0;
}

void VideoStream::setInputFormatOptions(std::unique_ptr<ffmpeg::InputFormat>& inputFormat)
{
    if (auto cam = m_camera.lock())
    {
        NX_DEBUG(this, "Camera %1 attempting to open video stream with params: %2",
            cam->toString(),
            m_codecParams.toString());
    }

    AVFormatContext * context = inputFormat->formatContext();

    if (m_codecParams.codecId != AV_CODEC_ID_NONE)
        context->video_codec_id = m_codecParams.codecId;

    // Dshow input format drops frames if its real time buffer gets too full.
    // If the camera is streaming in H264 format, dropping will corrupt the stream.
    // So, two things may help: instructing the AVFormatContext not to buffer packets when it can
    // avoid it, and increasing the default real time buffer size from 3041280 (3M) to 20M.
    context->flags |= AVFMT_FLAG_NOBUFFER; //< Could be helpful for Linux too.

    if (m_codecParams.fps > 0)
    {
        inputFormat->setFps(m_codecParams.fps);
    }

    if (m_codecParams.resolution.width * m_codecParams.resolution.height > 0)
    {
        inputFormat->setResolution(
            m_codecParams.resolution.width,
            m_codecParams.resolution.height);
    }

    // Note: setting the bitrate only works for raspberry pi mmal camera
    if (m_codecParams.bitrate > 0)
    {
        if (auto cam = m_camera.lock())
        {
            // ffmpeg doesn't have an option for setting the bitrate on AVFormatContext.
            device::video::setBitrate(
                ffmpegUrl(),
                m_codecParams.bitrate,
                cam->compressionTypeDescriptor());
        }
    }
}

std::shared_ptr<ffmpeg::Packet> VideoStream::readFrame()
{
    auto packet = std::make_shared<ffmpeg::Packet>(
        m_inputFormat->videoCodecId(),
        AVMEDIA_TYPE_VIDEO);

    int result;
    if (m_inputFormat->formatContext()->flags & AVFMT_FLAG_NONBLOCK)
    {
        using namespace std::chrono_literals;
        result = m_inputFormat->readFrameNonBlock(packet->packet(), 1000ms);
        if (result == AVERROR(EAGAIN))
            result = AVERROR(EIO); //< Treating a one second timeout as an io error.
    }
    else
    {
        result = m_inputFormat->readFrame(packet->packet());
    }

    checkIoError(result); // < Need to reset m_ioError if readFrame was successful.

    if (result < 0)
    {
        setLastError(result);
        if (m_ioError)
            m_terminated = true;
        return nullptr;
    }

#ifdef _WIN32
    // Dshow input format does not set h264 key packet flag correctly, so do it manually
    if (packet->codecId() == AV_CODEC_ID_H264 && isKeyFrame(packet.get()))
        packet->packet()->flags |= AV_PKT_FLAG_KEY;
#endif

    // Setting timestamp here because primary stream needs it even if there is no decoding.
    packet->setTimestamp(m_timeProvider->millisSinceEpoch());

    return packet;
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
    setCameraState(csModified);
}

CodecParameters VideoStream::findClosestHardwareConfiguration(const CodecParameters& params) const
{
    std::vector<device::video::ResolutionData>resolutionList;

    // Assumes list is in ascending resolution order
    if (auto cam = m_camera.lock())
        resolutionList = cam->resolutionList();

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
    for (const auto & resolution : resolutionList)
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
    for (const auto & resolution : resolutionList)
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
        setCameraState(csModified);
    }
}

void VideoStream::setCameraState(CameraState cameraState)
{
    m_streamState = cameraState;
}

bool VideoStream::checkIoError(int ffmpegError)
{
    m_ioError = ffmpegError == AVERROR(ENODEV) //< Linux.
        || ffmpegError == AVERROR(EIO) //< Windows.
        || ffmpegError == AVERROR(EBUSY); //< Device is busy during initialization.
    return m_ioError;
}

void VideoStream::setLastError(int ffmpegError)
{
    if (auto cam = m_camera.lock())
        cam->setLastError(ffmpegError);
}

} // namespace usb_cam
} // namespace nx
