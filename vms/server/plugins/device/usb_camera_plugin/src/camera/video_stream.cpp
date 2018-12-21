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
    if(!packet)
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
    m_timeProvider(camera.lock()->timeProvider()),
    m_packetCount(std::make_shared<std::atomic_int>(0)),
    m_frameCount(std::make_shared<std::atomic_int>(0))
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
    return m_actualFps > 0 ? m_actualFps.load() : m_codecParams.fps;
}

std::chrono::milliseconds VideoStream::timePerFrame() const
{
    return std::chrono::milliseconds((int)(1.0f / fps() * kMsecInSec));
}

std::chrono::milliseconds VideoStream::actualTimePerFrame() const
{
    return std::chrono::milliseconds((int)(1.0f / actualFps() * kMsecInSec));
}

AVPixelFormat VideoStream::decoderPixelFormat() const
{
    return (AVPixelFormat) m_decoderPixelFormat.load();
}

void VideoStream::addPacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    {
        std::lock_guard<std::mutex> lock(m_cameraMutex);

        static constexpr PacketConsumerManager::ConsumerState skipUntilNextKeyPacket =
            PacketConsumerManager::ConsumerState::skipUntilNextKeyPacket;

        m_packetConsumerManager.addConsumer(consumer, skipUntilNextKeyPacket);
        updateUnlocked();
    }
    m_consumerWaitCondition.notify_all();

    tryToStartIfNotStarted();
}

void VideoStream::removePacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    {
        std::lock_guard<std::mutex> lock(m_cameraMutex);
        m_packetConsumerManager.removeConsumer(consumer);
        updateUnlocked();
    }
    m_consumerWaitCondition.notify_all();
}

void VideoStream::addFrameConsumer(const std::weak_ptr<AbstractFrameConsumer>& consumer)
{
    {
        std::lock_guard<std::mutex> lock(m_cameraMutex);
        m_frameConsumerManager.addConsumer(consumer);
        updateUnlocked();
    }
    m_consumerWaitCondition.notify_all();

    tryToStartIfNotStarted();
}

void VideoStream::removeFrameConsumer(const std::weak_ptr<AbstractFrameConsumer>& consumer)
{
    {
        std::lock_guard<std::mutex> lock(m_cameraMutex);
        m_frameConsumerManager.removeConsumer(consumer);

        // Raspberry Pi: reinitialize the camera to recover frame rate on the primary stream.
        if (nx::utils::AppInfo::isRaspberryPi() && m_frameConsumerManager.empty())
            setCameraState(csModified);
    }
    m_consumerWaitCondition.notify_all();
}

void VideoStream::updateFps()
{
    std::lock_guard<std::mutex> lock(m_cameraMutex);
    updateFpsUnlocked();
}

void VideoStream::updateBitrate()
{
    std::lock_guard<std::mutex> lock(m_cameraMutex);
    updateBitrateUnlocked();
}

void VideoStream::updateResolution()
{
    std::lock_guard<std::mutex> lock(m_cameraMutex);
    updateResolutionUnlocked();
}

bool VideoStream::ioError() const
{
    return m_ioError;
}

bool VideoStream::pluggedIn() const
{
    return !device::video::getDeviceName(ffmpegUrl().c_str()).empty();
}

void VideoStream::updateActualFps(uint64_t now)
{
    ++ m_updatingFps;

    if (now - m_oneSecondAgo >= kMsecInSec)
    {
        m_actualFps = m_updatingFps.load();
        m_updatingFps = 0;
        m_oneSecondAgo = now;
    }
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

bool VideoStream::waitForConsumers()
{
    bool wait;
    {
        std::lock_guard<std::mutex> lock(m_cameraMutex);
        wait = noConsumers();
    }
    if (wait)
        uninitialize(); // < Don't stream the camera if there are no consumers

    // Check again if there are no consumers because they could have been added during unitialize.
    std::unique_lock<std::mutex> lock(m_cameraMutex);
    if(noConsumers())
    {
        m_consumerWaitCondition.wait(lock, [&]() { return m_terminated || !noConsumers(); });
        return !m_terminated;
    }

    return true;
}

bool VideoStream::noConsumers() const
{
    return m_packetConsumerManager.empty() && m_frameConsumerManager.empty();
}

void VideoStream::terminate()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_terminated = true;
    m_wait.notify_all();
}

void VideoStream::tryToStartIfNotStarted()
{
    std::lock_guard<std::mutex> lock(m_threadStartMutex);
    if (m_terminated)
    {
        if (pluggedIn()) 
            start();
    }
}

void VideoStream::start()
{
    if (m_videoThread.joinable())
        m_videoThread.join();

    m_terminated = false;
    m_videoThread = std::thread(&VideoStream::run, this);
}

void VideoStream::stop()
{
    terminate();

    m_consumerWaitCondition.notify_all();
    if (m_videoThread.joinable())
        m_videoThread.join();
}

void VideoStream::run()
{
    while (!m_terminated)
    {
        if (!waitForConsumers())    
            continue;

        if (!ensureInitialized())
            continue;

        auto packet = readFrame();
        if (!packet)
            continue;

        auto frame = maybeDecode(packet.get());

        updateActualFps(m_timeProvider->millisSinceEpoch());

        {
            std::lock_guard<std::mutex> lock(m_cameraMutex);
            m_packetConsumerManager.givePacket(packet);
            if (frame)
                m_frameConsumerManager.giveFrame(frame);
        }
    }

    uninitialize();
}

bool VideoStream::ensureInitialized()
{
    bool needInit;
    bool needUninit;
    {
        std::lock_guard<std::mutex> lock(m_cameraMutex);
        needUninit = m_cameraState == csModified;
        needInit = needUninit || m_cameraState == csOff;
    }

    if (needUninit)
        uninitialize();

    if (needInit)
    {
        m_initCode = initialize();
        checkIoError(m_initCode);
        if(m_initCode < 0)
        {
            setLastError(m_initCode);
            if (m_ioError)
                terminate();
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_cameraMutex);
        return m_cameraState == csInitialized;
    }
}

int VideoStream::initialize()
{
    std::lock_guard<std::mutex> lock(m_cameraMutex);

    int result = initializeInputFormat();
    if (result < 0)
        return result;

    result = initializeDecoder();
    if (result < 0)
        return result;

    setCameraState(csInitialized);

    return 0;
}

void VideoStream::uninitialize()
{
    {
        std::lock_guard<std::mutex> lock(m_cameraMutex);
        // Flushing to reduce m_packetCount and m_frameCount before sleepy loop below.
        m_packetConsumerManager.flush();
        m_frameConsumerManager.flush();
    }

    // Make sure not to keep m_cameraMutex locked while sleeping here.

    // Some cameras segfault if they are unintialized while there are still packets and / or frames
    // allocated. They own the memory being referred to by the packet, so the packets / frames
    // need to be deallocated first. Beware, any strong references to the packets should be
    // released ASAP to avoid endless spinning here.
    while (*m_packetCount > 0 || *m_frameCount > 0)
    {
        static constexpr std::chrono::milliseconds kSleep(30);
        std::this_thread::sleep_for(kSleep);
    }

    {
        std::lock_guard<std::mutex> lock(m_cameraMutex);

        // flush the decoder to avoid memory ownership problems described above.
        if (m_decoder)
            m_decoder->flush();

        m_decoder.reset(nullptr);
        m_decoderPixelFormat = -1;
        m_inputFormat.reset(nullptr);

        setCameraState(csOff);
    }
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
        NX_DEBUG(
            this,
            "%1: Camera %2 attempting to open video stream with params: %3",
            __func__,
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
#ifdef _WIN32
    static constexpr int kRealTimeBufferSize = 20000000; //< 20 Megabytes.
    if(context->video_codec_id == AV_CODEC_ID_H264)
        context->max_picture_buffer = kRealTimeBufferSize;
#endif

    if (m_codecParams.fps > 0)
        inputFormat->setFps(m_codecParams.fps);

    if (m_codecParams.resolution.width * m_codecParams.resolution.height > 0)
    {
        inputFormat->setResolution(
            m_codecParams.resolution.width,
            m_codecParams.resolution.height);
    }

    // Note: setting the bitrate only works for raspberry pi mmal camera
    if (m_codecParams.bitrate > 0)
    {
        if(auto cam = m_camera.lock())
        {
            // ffmpeg doesn't have an option for setting the bitrate on AVFormatContext.
            device::video::setBitrate(
                ffmpegUrl(),
                m_codecParams.bitrate,
                cam->compressionTypeDescriptor());
        }
    }
}

int VideoStream::initializeDecoder()
{
    auto decoder = std::make_unique<ffmpeg::Codec>();

    int result;
    if (nx::utils::AppInfo::isRaspberryPi() && m_codecParams.codecId == AV_CODEC_ID_H264)
    {
        result = decoder->initializeDecoder("h264_mmal");
    }
    else
    {
        AVStream * stream = m_inputFormat->findStream(AVMEDIA_TYPE_VIDEO);
        result = stream
            ? decoder->initializeDecoder(stream->codecpar)
            : AVERROR_DECODER_NOT_FOUND; //< Using as stream not found.
    }
    if (result < 0)
        return result;

    result = decoder->open();
    if (result < 0)
        return result;

    m_skipUntilNextKeyPacket = true;
    m_decoder = std::move(decoder);

    m_decoderPixelFormat = m_decoder->pixelFormat();

    return 0;
}

std::shared_ptr<ffmpeg::Packet> VideoStream::readFrame()
{
    auto packet = std::make_shared<ffmpeg::Packet>(
        m_inputFormat->videoCodecID(),
        AVMEDIA_TYPE_VIDEO,
        m_packetCount);

    int result = m_inputFormat->readFrame(packet->packet());

    checkIoError(result); // < Need to reset m_ioError if readFrame was successful.

    if (result < 0)
    {
        setLastError(result);
        if (m_ioError)
            terminate();
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

std::shared_ptr<ffmpeg::Frame> VideoStream::maybeDecode(const ffmpeg::Packet * packet)
{
    {
        std::lock_guard<std::mutex> lock(m_cameraMutex);
        if (m_frameConsumerManager.empty())
            return nullptr;
    }

    if (m_skipUntilNextKeyPacket)
    {
        if (!packet->keyPacket())
            return nullptr;
        m_skipUntilNextKeyPacket = false;
    }

    auto frame = std::make_shared<ffmpeg::Frame>(m_frameCount);
    int result = decode(packet, frame.get());
    if (result < 0)
        return nullptr;

    if (frame->pts() == AV_NOPTS_VALUE)
        frame->frame()->pts = frame->packetPts();

    m_timestamps.addTimestamp(packet->pts(), packet->timestamp());

    auto nxTimestamp = m_timestamps.takeNxTimestamp(frame->packetPts());
    if (!nxTimestamp.has_value())
        nxTimestamp.emplace(m_timeProvider->millisSinceEpoch());

    frame->setTimestamp(nxTimestamp.value());

    return frame;
}

int VideoStream::decode(const ffmpeg::Packet * packet, ffmpeg::Frame * frame)
{
    int result = m_decoder->sendPacket(packet->packet());
    if(result == 0 || result == AVERROR(EAGAIN))
        return m_decoder->receiveFrame(frame->frame());
    return result;
}

float VideoStream::findLargestFps() const
{
    if(noConsumers())
        return 0;

    float packetFps = m_packetConsumerManager.findLargestFps();
    float frameFps = m_frameConsumerManager.findLargestFps();

    return frameFps > packetFps ? frameFps : packetFps;
}

nxcip::Resolution VideoStream::findLargestResolution() const
{
    if(noConsumers())
        return {};

    nxcip::Resolution largest;
    auto packetResolution = m_packetConsumerManager.findLargestResolution();
    auto frameResolution = m_frameConsumerManager.findLargestResolution();

    if(frameResolution.width * frameResolution.height > 
        packetResolution.width * packetResolution.height)
    {
        largest = frameResolution;
    }
    else
    {
        largest = packetResolution;
    }

    return largest;
}

int VideoStream::findLargestBitrate() const
{
    if (noConsumers())
        return 0;

    int packetBitrate = m_packetConsumerManager.findLargestBitrate();
    int frameBitrate = m_frameConsumerManager.findLargestBitrate();

    return frameBitrate > packetBitrate ? frameBitrate : packetBitrate;
}

void VideoStream::updateFpsUnlocked()
{
    if (noConsumers())
        return;

    CodecParameters newParams = m_codecParams;
    newParams.fps = findLargestFps();

    CodecParameters finalParams = findClosestHardwareConfiguration(newParams);
    setCodecParameters(finalParams);
}

void VideoStream::updateResolutionUnlocked()
{
    if (noConsumers())
        return;

    CodecParameters newParams = m_codecParams;
    newParams.resolution = findLargestResolution();

    auto finalParams = findClosestHardwareConfiguration(newParams);
    setCodecParameters(finalParams);
}

void VideoStream::updateBitrateUnlocked()
{
    if (noConsumers())
        return;

    int bitrate = findLargestBitrate();

    if (m_codecParams.bitrate != bitrate)
    {
        m_codecParams.bitrate = bitrate;
        setCameraState(csModified);
    }
}

void VideoStream::updateUnlocked()
{   
    if(noConsumers())
        return;

    // Could call updateFpsUnlocked() and updateResolutionUnlcoked() here, but this way
    // findClosestHardwareConfiguration(), which queries hardware, only gets called once
    CodecParameters newParams = m_codecParams;

    newParams.fps = findLargestFps();
    newParams.resolution =  findLargestResolution();
    updateBitrateUnlocked();

    CodecParameters finalParams = findClosestHardwareConfiguration(newParams);
    setCodecParameters(finalParams);
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
        if (resolution.width == params.resolution.width
            && resolution.height == params.resolution.height 
            && resolution.fps == params.fps)
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
    m_cameraState = cameraState;
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
