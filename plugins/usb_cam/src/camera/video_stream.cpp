#include "video_stream.h"

#include <algorithm>

#include <nx/utils/log/log.h>
#include <nx/utils/app_info.h>
#include <plugins/plugin_container_api.h>

#include "camera.h"
#include "buffered_stream_consumer.h"
#include "device/utils.h"
#include "ffmpeg/utils.h"

namespace nx {
namespace usb_cam {

namespace {

const char * deviceType()
{
    return
#ifdef _WIN32
        "dshow";
#elif __linux__
        "v4l2";
#elif __APPLE__
        "avfoundation";
#else
        "";
#endif
}

static constexpr int kMsecInSec = 1000;

} // namespace

VideoStream::VideoStream(
    const std::string& url,
    const CodecParameters& codecParams,
    const std::weak_ptr<Camera>& camera)
    :
    m_url(url),
    m_codecParams(codecParams),
    m_camera(camera),
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

std::string VideoStream::url() const
{
    return m_url;
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
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_decoder ? m_decoder->pixelFormat() : AV_PIX_FMT_NONE;
}

void VideoStream::addPacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_packetConsumerManager.addConsumer(consumer, true/*waitForKeyPacket*/);
    updateUnlocked();
    tryStart();
    m_wait.notify_all();
}

void VideoStream::removePacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_packetConsumerManager.removeConsumer(consumer);
    updateUnlocked();
}

void VideoStream::addFrameConsumer(const std::weak_ptr<AbstractFrameConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frameConsumerManager.addConsumer(consumer);
    updateUnlocked();
    tryStart();
    m_wait.notify_all();
}

void VideoStream::removeFrameConsumer(const std::weak_ptr<AbstractFrameConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frameConsumerManager.removeConsumer(consumer);
    updateUnlocked();
}

void VideoStream::updateFps()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    updateFpsUnlocked();
}

void VideoStream::updateBitrate()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    updateBitrateUnlocked();
}

void VideoStream::updateResolution()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    updateResolutionUnlocked();
}

bool VideoStream::ioError() const
{
    return m_ioError;
}

bool VideoStream::pluggedIn() const
{
    return !device::getDeviceName(m_url.c_str()).empty();
}

void VideoStream::updateActualFps(uint64_t now)
{
    if(now - m_oneSecondAgo <= kMsecInSec)
    {
        ++m_updatingFps;
    }
    else
    {
        m_actualFps = m_updatingFps.load();
        m_updatingFps = 0;
        m_oneSecondAgo = now;
    }
}

std::string VideoStream::ffmpegUrl() const
{
    return
#ifdef _WIN32
        std::string("video=@device_pnp_") + m_url;
#else
        m_url;
#endif
}

bool VideoStream::consumersEmpty() const
{
    return m_packetConsumerManager.empty() && m_frameConsumerManager.empty();
}

bool VideoStream::waitForConsumers()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (consumersEmpty())
    {
        uninitialize();
        m_wait.wait(lock, [&]() { return m_terminated || !consumersEmpty(); });
    }
    return !m_terminated;
}

void VideoStream::tryStart()
{
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
    m_terminated = true;
    m_wait.notify_all();
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
        {
            NX_DEBUG(this) << m_url << "Unable to initialize VideoStream: " 
                << ffmpeg::utils::errorToString(m_initCode);
            break;   
        }

        auto packet = readFrame();
        if (!packet)
            continue;

        // Setting timestamp here because primary stream needs it even if there is no decoding.
        packet->setTimestamp(m_timeProvider->millisSinceEpoch());

        auto frame = maybeDecode(packet.get());

        uint64_t now = m_timeProvider->millisSinceEpoch();

        std::lock_guard<std::mutex> lock(m_mutex);        
        updateActualFps(now);
        m_packetConsumerManager.givePacket(packet);
        if (frame)
            m_frameConsumerManager.giveFrame(frame);
    }

    uninitialize();
}

bool VideoStream::ensureInitialized()
{
    bool needInit = m_cameraState == csModified || m_cameraState == csOff;
    if (m_cameraState == csModified)
    {
        std::lock_guard<std::mutex>lock(m_mutex);
        uninitialize();
    }

    if (needInit)
    {
        std::lock_guard<std::mutex>lock(m_mutex);
        m_initCode = initialize();
        if (m_initCode < 0)
        {
            if (auto cam = m_camera.lock())
                cam->setLastError(m_initCode);
        }
    }

    return m_cameraState == csInitialized;
}

int VideoStream::initialize()
{
    int result = initializeInputFormat();
    if (result < 0)
        return result;

    result = initializeDecoder();
    if (result < 0)
        return result;

    m_cameraState = csInitialized;
    return 0;
}

void VideoStream::uninitialize()
{
    m_packetConsumerManager.flush();
    m_frameConsumerManager.flush();

    // Some cameras segfault if they are unintialized while there are still packets and / or frames
    // allocated. They own the memory being referred to be the packet, so the packet needs to go
    // be deallocated first.
    while (*m_packetCount > 0 || *m_frameCount > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

    if (m_decoder)
        m_decoder->flush();
    m_decoder.reset(nullptr);
    m_inputFormat.reset(nullptr);

    m_cameraState = csOff;
}

int VideoStream::initializeInputFormat()
{
    auto inputFormat = std::make_unique<ffmpeg::InputFormat>();

    int result = inputFormat->initialize(deviceType());
    if (result < 0)
        return result;

    setInputFormatOptions(inputFormat);

    result = inputFormat->open(ffmpegUrl().c_str());
    checkIoError(result);
    if (result < 0)
        return result;

    m_inputFormat = std::move(inputFormat);
    return 0;
}

void VideoStream::setInputFormatOptions(std::unique_ptr<ffmpeg::InputFormat>& inputFormat)
{
    AVFormatContext * context = inputFormat->formatContext();
    if (m_codecParams.codecId != AV_CODEC_ID_NONE)
        context->video_codec_id = m_codecParams.codecId;

    context->flags |= AVFMT_FLAG_DISCARD_CORRUPT | AVFMT_FLAG_NOBUFFER;

    if (m_codecParams.fps > 0)
        inputFormat->setFps(m_codecParams.fps);

    if (m_codecParams.width * m_codecParams.height > 0)
        inputFormat->setResolution(m_codecParams.width, m_codecParams.height);

    // Note: setting the bitrate only works for raspberry pi mmal camera
    if (m_codecParams.bitrate > 0)
    {
        if(auto cam = m_camera.lock())
        {
            // ffmpeg doesn't have an option for setting the bitrate on AVFormatContext.
            device::setBitrate(
                m_url.c_str(),
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
            : AVERROR_DECODER_NOT_FOUND;//< Using as stream not found
    }
    if (result < 0)
        return result;

    result = decoder->open();
    if (result < 0)
        return result;

    m_waitForKeyPacket = true;
    m_decoder = std::move(decoder);

    // Some cameras have infrequent I-frames. Initializing the decoder with the first packet read
    // from the camera (presumably an I-frame) prevents numerous "non exisiting PPS 0 referenced"
    // errors on some cameras with h264, on Windows in particular.
    ffmpeg::Packet packet(m_inputFormat->videoCodecID(), AVMEDIA_TYPE_VIDEO);
    if (m_inputFormat->readFrame(packet.packet()) == 0)
    {
        ffmpeg::Frame frame;
        decode(&packet, &frame);
    }

    return 0;
}

std::shared_ptr<ffmpeg::Packet> VideoStream::readFrame()
{
    auto packet = std::make_shared<ffmpeg::Packet>(
        m_inputFormat->videoCodecID(),
        AVMEDIA_TYPE_VIDEO,
        m_packetCount);

    int result = m_inputFormat->readFrame(packet->packet());
    if (result < 0)
    {
        if (checkIoError(result))
        {
            m_terminated = true;
            if(auto cam = m_camera.lock())
                cam->setLastError(result);
        }
        return nullptr;
    }
    return packet;
}

std::shared_ptr<ffmpeg::Frame> VideoStream::maybeDecode(const ffmpeg::Packet * packet)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_frameConsumerManager.empty())
            return nullptr;
    }

    if (m_waitForKeyPacket)
    {
        if (!packet->keyPacket())
            return nullptr;
        m_waitForKeyPacket = false;
    }

    auto frame = std::make_shared<ffmpeg::Frame>(m_frameCount);
    int result = decode(packet, frame.get());
    if (result < 0)
        return nullptr;

    if (frame->pts() == AV_NOPTS_VALUE)
        frame->frame()->pts = frame->packetPts();

    // Don't allow too many dangling timestamps.
    if (m_timestamps.size() > 30)
        m_timestamps.clear();

    m_timestamps.addTimestamp(packet->pts(), packet->timestamp());

    uint64_t nxTimestamp = 0;
    if (!m_timestamps.getNxTimestamp(frame->packetPts(), &nxTimestamp, true /*eraseEntry*/))
        nxTimestamp = m_timeProvider->millisSinceEpoch();

    frame->setTimestamp(nxTimestamp);

    return frame;
}

int VideoStream::decode(const ffmpeg::Packet * packet, ffmpeg::Frame * frame)
{
    int result = m_decoder->sendPacket(packet->packet());
    if(result == 0 || result == AVERROR(EAGAIN))
        return m_decoder->receiveFrame(frame->frame());
    return result;
}

float VideoStream::largestFps() const
{
    if(consumersEmpty())
        return 0;

    float packetFps = 0;
    m_packetConsumerManager.largestFps(&packetFps);

    float frameFps = 0;
    m_frameConsumerManager.largestFps(&frameFps);

    return frameFps > packetFps ? frameFps : packetFps;
}

void VideoStream::largestResolution(int * outWidth, int * outHeight) const
{
    *outWidth = 0;
    *outHeight = 0;

    if(consumersEmpty())
        return;

    int packetConsumerWidth = 0;
    int packetConsumerHeight = 0;
    m_packetConsumerManager.largestResolution(&packetConsumerWidth, &packetConsumerHeight);

    int frameConsumerWidth = 0;
    int frameConsumerHeight = 0;
    m_frameConsumerManager.largestResolution(&frameConsumerWidth, &frameConsumerHeight);

    if(frameConsumerWidth * frameConsumerHeight > packetConsumerWidth * packetConsumerHeight)
    {
        *outWidth = frameConsumerWidth;
        *outHeight = frameConsumerHeight;
    }
    else
    {
        *outWidth = packetConsumerWidth;
        *outHeight = packetConsumerHeight;
    }
}

int VideoStream::largestBitrate() const
{
    if (consumersEmpty())
        return 0;

    int packetBitrate = 0;
    m_packetConsumerManager.largestBitrate(&packetBitrate);

    int frameBitrate = 0;
    m_frameConsumerManager.largestBitrate(&frameBitrate);

    return frameBitrate > packetBitrate ? frameBitrate : packetBitrate;
}

void VideoStream::updateFpsUnlocked()
{
    if (consumersEmpty())
        return;

    CodecParameters newParams = m_codecParams;
    newParams.fps = largestFps();

    CodecParameters codecParams = closestHardwareConfiguration(newParams);
    setCodecParameters(codecParams);
}

void VideoStream::updateResolutionUnlocked()
{
    if (consumersEmpty())
        return;

    int width = 0;
    int height = 0;
    CodecParameters newParams = m_codecParams;
    largestResolution(&width, &height);
    newParams.setResolution(width, height);

    CodecParameters codecParams = closestHardwareConfiguration(newParams);
    setCodecParameters(codecParams);
}

void VideoStream::updateBitrateUnlocked()
{
    if (consumersEmpty())
        return;

    int bitrate = largestBitrate();

    if (m_codecParams.bitrate != bitrate)
    {
        m_codecParams.bitrate = bitrate;
        m_cameraState = csModified;
    }
}

void VideoStream::updateUnlocked()
{   
    if(consumersEmpty())
        return;

    // Could call updateFpsUnlocked() and updateResolutionUnlcoked() here, but this way
    // closestHardwareConfiguration(), which queries hardware, only gets called once
    CodecParameters newParams = m_codecParams;
    newParams.fps = largestFps();

    int width = 0;
    int height = 0;
    largestResolution(&width, &height);
    newParams.setResolution(width, height);

    updateBitrateUnlocked();

    CodecParameters codecParams = closestHardwareConfiguration(newParams);
    setCodecParameters(codecParams);

    NX_DEBUG(this) << m_url + ":" << "Selected Params:" << m_codecParams.toString();
}

CodecParameters VideoStream::closestHardwareConfiguration(const CodecParameters& params) const
{
    std::vector<device::ResolutionData>resolutionList;

    // Assumes list is in ascending resolution order
    if (auto cam = m_camera.lock())
        resolutionList = cam->resolutionList();

    // Try to find an exact match first
    for (const auto & resolution : resolutionList)
    {
        if (resolution.width == params.width 
            && resolution.height == params.height 
            && resolution.fps == params.fps)
        {
            return CodecParameters(
                m_codecParams.codecId, 
                params.fps,
                m_codecParams.bitrate,
                params.width,
                params.height);
        }
    }

    // Then a match with similar aspect ratio whose resolution and fps are higher than requested
    float aspectRatio = (float) params.width / params.height;
    for (const auto & resolution : resolutionList)
    {
        if (aspectRatio == resolution.aspectRatio())
        {
            if (resolution.width * resolution.height >= params.width * params.height 
                && resolution.fps >= params.fps)
            {
                return CodecParameters(
                    m_codecParams.codecId,
                    resolution.fps,
                    m_codecParams.bitrate,
                    resolution.width,
                    resolution.height);
            }
        }
    }

    // Any resolution or fps higher than requested
    for (const auto & resolution : resolutionList)
    {
        if (resolution.width * resolution.height >= params.width * params.height 
            && resolution.fps >= params.fps)
        {
            return CodecParameters(
                m_codecParams.codecId,
                resolution.fps,
                m_codecParams.bitrate,
                resolution.width,
                resolution.height);
        }
    }

    return m_codecParams;
}

void VideoStream::setCodecParameters(const CodecParameters& codecParams)
{
    if (m_codecParams.fps != codecParams.fps
        || m_codecParams.width * m_codecParams.height != codecParams.width * codecParams.height)
    {
        AVCodecID codecId = m_codecParams.codecId;
        m_codecParams = codecParams;
        m_codecParams.codecId = codecId;
        m_cameraState = csModified;
    }
}

bool VideoStream::checkIoError(int ffmpegError)
{
    // TODO: find out if this is the error code on windows
    m_ioError = ffmpegError == AVERROR(ENODEV);
    return m_ioError;
}

} // namespace usb_cam
} // namespace nx