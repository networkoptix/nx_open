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

void setBitrate(const char * devicePath, int bitrate, nxcip::CompressionType codecID)
{
    device::setBitrate(devicePath, bitrate, codecID);
}

const int kRetryLimit = 10;

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
    m_cameraState(csOff),
    m_waitForKeyPacket(true),
    m_packetCount(std::make_shared<std::atomic_int>(0)),
    m_frameCount(std::make_shared<std::atomic_int>(0)),
    m_terminated(false),
    m_retries(0),
    m_initCode(0)
{
    start();
}

VideoStream::~VideoStream()
{
    stop();
    uninitialize();
    m_timeProvider->releaseRef();
}

void VideoStream::addPacketConsumer(const std::weak_ptr<PacketConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_packetConsumerManager.addConsumer(consumer, true/*waitForKeyPacket*/);
    updateUnlocked();
    m_wait.notify_all();
}

void VideoStream::removePacketConsumer(const std::weak_ptr<PacketConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_packetConsumerManager.removeConsumer(consumer);
    updateUnlocked();
}

void VideoStream::addFrameConsumer(const std::weak_ptr<FrameConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t pos = m_frameConsumerManager.addConsumer(consumer);
    updateUnlocked();
    m_wait.notify_all();
}

void VideoStream::removeFrameConsumer(const std::weak_ptr<FrameConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frameConsumerManager.removeConsumer(consumer);
    updateUnlocked();
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

void VideoStream::waitForConsumers()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (consumersEmpty())
    {
        uninitialize();
        m_wait.wait(lock, [&]() { return m_terminated || !consumersEmpty(); });
    }
}

void VideoStream::updateFpsUnlocked()
{
    if (consumersEmpty())
        return;

    float packetFps = 0;
    float frameFps = 0;
    std::weak_ptr<VideoConsumer> packetConsumer;
    std::weak_ptr<VideoConsumer> frameConsumer;

    packetConsumer = m_packetConsumerManager.largestFps(&packetFps);
    frameConsumer = m_frameConsumerManager.largestFps(&frameFps);

    auto videoConsumer = frameFps > packetFps ? frameConsumer : packetConsumer;
    if (!videoConsumer.lock())
        return;

    CodecParameters codecParams = closestSupportedResolution(videoConsumer);
    updateStreamConfiguration(codecParams);
}

void VideoStream::updateResolutionUnlocked()
{
    if (consumersEmpty())
        return;

    int packetWidth = 0;
    int packetHeight = 0;
    std::weak_ptr<VideoConsumer> packetConsumer;
    packetConsumer = m_packetConsumerManager.largestResolution(&packetWidth, &packetHeight);

    int frameWidth = 0;
    int frameHeight = 0;
    std::weak_ptr<VideoConsumer> frameConsumer;
    frameConsumer = m_frameConsumerManager.largestResolution(&frameWidth, &frameHeight);

    std::weak_ptr<VideoConsumer> videoConsumer = packetConsumer;
    if (frameWidth * frameHeight > frameWidth * frameHeight)
        videoConsumer = frameConsumer.lock() ? frameConsumer : videoConsumer;

    if (!videoConsumer.lock())
        return;

    CodecParameters codecParams = closestSupportedResolution(videoConsumer);
    updateStreamConfiguration(codecParams);
}

void VideoStream::updateBitrateUnlocked()
{
    if (consumersEmpty())
        return;

    int packetBitrate = 0;
    int frameBitrate = 0;
    std::weak_ptr<VideoConsumer>packetConsumer;
    std::weak_ptr<VideoConsumer>frameConsumer;

    packetConsumer = m_packetConsumerManager.largestBitrate(&packetBitrate);
    frameConsumer = m_frameConsumerManager.largestBitrate(&frameBitrate);

    std::weak_ptr<VideoConsumer> videoConsumer = frameBitrate > packetBitrate 
        ? packetConsumer 
        : frameConsumer;

    if (!videoConsumer.lock())
        return;
    
    if (m_codecParams.bitrate != videoConsumer.lock()->bitrate())
    {
        m_codecParams.bitrate = videoConsumer.lock()->bitrate();
        m_cameraState = csModified;
    }
}

void VideoStream::updateUnlocked()
{
    updateFpsUnlocked();
    updateResolutionUnlocked();
    updateBitrateUnlocked();
    NX_DEBUG(this) << m_url + ":" << "Selected Params:" << m_codecParams.toString();
}

void VideoStream::start()
{
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

AVPixelFormat VideoStream::decoderPixelFormat() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_decoder ? m_decoder->pixelFormat() : AV_PIX_FMT_NONE;
}

int VideoStream::fps() const
{
    return m_codecParams.fps;
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

std::string VideoStream::url() const
{
    return m_url;
}

void VideoStream::run()
{
    while (!m_terminated)
    {
        if (m_retries >= kRetryLimit)
        {
            NX_DEBUG(this) << m_url << "exceeded retry limit of " << kRetryLimit
                << ". Error:" << ffmpeg::utils::errorToString(m_initCode);
            return;
        }

        waitForConsumers();
        if (m_terminated)
            return;

        if (!ensureInitialized())
        {
            ++m_retries;
            continue;
        }

        auto packet = std::make_shared<ffmpeg::Packet>(m_inputFormat->videoCodecID(), m_packetCount);
        int readCode = m_inputFormat->readFrame(packet->packet());
        if (readCode < 0)
            continue;

        packet->setTimeStamp(m_timeProvider->millisSinceEpoch());
        m_timeStamps.addTimeStamp(packet->pts(), packet->timeStamp());

        auto frame = maybeDecode(packet.get());

        std::lock_guard<std::mutex> lock(m_mutex);
        m_packetConsumerManager.givePacket(packet);
        if (frame)
            m_frameConsumerManager.giveFrame(frame);
    }
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
    m_packetConsumerManager.consumerFlush();
    m_frameConsumerManager.consumerFlush();

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
    std::string audio = m_camera.lock()->audioStream()->url();
    audio = std::string("audio ") + audio;
    std::cout << audio << std::endl;
    if (result < 0)
        return result;

    m_inputFormat = std::move(inputFormat);
    return 0;
}

void VideoStream::setInputFormatOptions(std::unique_ptr<ffmpeg::InputFormat>& inputFormat)
{
    AVFormatContext * context = inputFormat->formatContext();
    if (m_codecParams.codecID != AV_CODEC_ID_NONE)
        context->video_codec_id = m_codecParams.codecID;

    context->flags |= AVFMT_FLAG_DISCARD_CORRUPT | AVFMT_FLAG_NOBUFFER;

    if (m_codecParams.fps > 0)
        inputFormat->setFps(m_codecParams.fps);

    if (m_codecParams.width * m_codecParams.height > 0)
        inputFormat->setResolution(m_codecParams.width, m_codecParams.height);

    if (m_codecParams.bitrate > 0)
    {
        /**
         * ffmpeg doesn't have an option for setting the bitrate on AVFormatContext.
         */
        setBitrate(
            m_url.c_str(),
            m_codecParams.bitrate,
            ffmpeg::utils::toNxCompressionType(m_codecParams.codecID));
    }

#ifdef WIN32
    /**
     * Attempt to avoid "real-time buffer too full" error messages in Windows
     */
    inputFormat->setEntry("thread_queue_size", 2048);
    inputFormat->setEntry("rtbufsize", "100M");
    inputFormat->setEntry("threads", (int64_t)0);
#endif
}

int VideoStream::initializeDecoder()
{
    auto decoder = std::make_unique<ffmpeg::Codec>();
    int initCode;
    if (nx::utils::AppInfo::isRaspberryPi() && m_codecParams.codecID == AV_CODEC_ID_H264)
    {
        initCode = decoder->initializeDecoder("h264_mmal");
    }
    else
    {
        AVStream * stream = m_inputFormat->findStream(AVMEDIA_TYPE_VIDEO);
        initCode = stream
            ? decoder->initializeDecoder(stream->codecpar)
            : AVERROR_DECODER_NOT_FOUND;
    }
    if (initCode < 0)
        return initCode;

    int openCode = decoder->open();
    if (openCode < 0)
        return openCode;

    m_waitForKeyPacket = true;
    m_decoder = std::move(decoder);

#ifdef _WIN32
    /**
     * dshow implementation does not set the key packet flag for sps/pps frames that are not IFrames,
     * and on some cameras they are very infrequent. Initializing the decoder with the first packet read from the 
     * camera prevent numerous "non exisiting PPS 0 referenced" errors on some cameras.
     */
    ffmpeg::Packet packet(m_inputFormat->videoCodecID());
    m_inputFormat->readFrame(packet.packet());
    ffmpeg::Frame frame;
    decode(&packet, &frame);
#endif

    return 0;
}

std::shared_ptr<ffmpeg::Frame> VideoStream::maybeDecode(const ffmpeg::Packet * packet)
{
    bool shouldDecode;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        shouldDecode = m_frameConsumerManager.size() > 0;
    }

    if (!shouldDecode)
        return nullptr;

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

    int64_t nxTimeStamp;
    if (!m_timeStamps.getNxTimeStamp(frame->pts(), &nxTimeStamp, true/*eraseEntry*/))
        nxTimeStamp = m_timeProvider->millisSinceEpoch();
    frame->setTimeStamp(nxTimeStamp);

    return frame;
}

int VideoStream::decode(const ffmpeg::Packet * packet, ffmpeg::Frame * frame)
{
    int result = 0;
    bool gotFrame = false;
    while (!gotFrame)
    {
        result = m_decoder->sendPacket(packet->packet());
        bool needReceive = result == 0 || result == AVERROR(EAGAIN);
        while (needReceive) // ready to or requires parsing a frame
        {
            result = m_decoder->receiveFrame(frame->frame());
            if (result == 0)
            {
                // got a frame or need to send more packets to decoder
                gotFrame = true;
                break;
            }
            else // something very bad happened or the decoder needs more input
            {
                return result;
            }
        }
        if (!needReceive) // something very bad happened
        {
            return result;
        }
    }

    return result;
}

CodecParameters VideoStream::closestSupportedResolution(const std::weak_ptr<VideoConsumer>& consumer) const
{
    NX_ASSERT(consumer.lock());
    if (!consumer.lock())
        return CodecParameters();

    auto consumerPtr = consumer.lock();

    std::vector<device::ResolutionData>resolutionList;
    if (auto cam = m_camera.lock())
        resolutionList = cam->getResolutionList();

    int width = 0;
    int height = 0;
    float fps = consumerPtr->fps();
    consumerPtr->resolution(&width, &height);

    // try to find an exact match first
    for (const auto & resolution : resolutionList)
    {
        if (fps == resolution.fps && width == resolution.width && height == resolution.height)
            return CodecParameters(
                m_codecParams.codecID, fps, consumerPtr->bitrate(), width, height);
    }

    // then a match with similar aspect ratio whose resolution and fps are higher than requested
    float aspectRatio = (float) width / height;
    for (const auto & resolution : resolutionList)
    {
        if (aspectRatio == resolution.aspectRatio())
        {
            if (width * height >= resolution.width * resolution.height && resolution.fps >= fps)
                return CodecParameters(
                    m_codecParams.codecID, fps, consumerPtr->bitrate(), width, height);
        }
    }

    //any resolution or fps higher than requested
    for (const auto & resolution : resolutionList)
    {
        if (width * height >= resolution.width * resolution.height && resolution.fps >= fps)
            return CodecParameters(
                m_codecParams.codecID, fps, consumerPtr->bitrate(), width, height);
    }

    return m_codecParams;
}

void VideoStream::updateStreamConfiguration(const CodecParameters& codecParams)
{
    if (m_codecParams.fps != codecParams.fps
        || m_codecParams.width * m_codecParams.height != codecParams.width * codecParams.height)
    {
        AVCodecID codecID = m_codecParams.codecID;
        m_codecParams = codecParams;
        m_codecParams.codecID = codecID;
        m_cameraState = csModified;
    }
}

} // namespace usb_cam
} // namespace nx