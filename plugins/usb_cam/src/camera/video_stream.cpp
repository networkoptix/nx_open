#include "video_stream.h"

#include <algorithm>
#ifdef __linux__
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#endif

#include <nx/utils/log/log.h>
#include <nx/utils/app_info.h>
#include <plugins/plugin_container_api.h>

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
    nxpl::TimeProvider *const timeProvider)
    :
    m_url(url),
    m_codecParams(codecParams),
    m_timeProvider(timeProvider),
    m_cameraState(kOff),
    m_waitForKeyPacket(true),
    m_packetCount(std::make_shared<std::atomic_int>(0)),
    m_frameCount(std::make_shared<std::atomic_int>(0)),
    m_terminated(false),
    m_retries(0),
    m_initCode(0)
{
    m_timeProvider->addRef();
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
    m_frameConsumerManager.addConsumer(consumer);
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

    float largest = std::max<float>(
        m_packetConsumerManager.largestFps(),
        m_frameConsumerManager.largestFps());
    
    if (m_codecParams.fps != largest)
    {
        m_codecParams.fps = largest;
        m_cameraState = kModified;
    }
}

void VideoStream::updateResolutionUnlocked()
{
    if (consumersEmpty())
        return;

    int largestWidth;
    int largestHeight;
    m_packetConsumerManager.largestResolution(&largestWidth, &largestHeight);

    int frameWidth;
    int frameHeight;
    m_frameConsumerManager.largestResolution(&frameWidth, &frameHeight);

    if(largestWidth * largestHeight < frameWidth * frameHeight)
    {
        largestWidth = frameWidth;
        largestHeight = frameHeight;
    }

    if(m_codecParams.width * m_codecParams.height != largestWidth * largestHeight)
    {
        m_codecParams.setResolution(largestWidth, largestHeight);
        m_cameraState = kModified;
    }
}

void VideoStream::updateBitrateUnlocked()
{
    if (consumersEmpty())
        return;

    int largest = std::max<int>(
        m_packetConsumerManager.largestBitrate(),
        m_frameConsumerManager.largestBitrate()
    );

    if (largest != m_codecParams.bitrate)
    {
        m_codecParams.bitrate = largest;
        m_cameraState = kModified;
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
    if(m_videoThread.joinable())
        m_videoThread.join();
}

AVPixelFormat VideoStream::decoderPixelFormat() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_decoder ? m_decoder->pixelFormat() : AV_PIX_FMT_NONE;
}

int VideoStream::gopSize() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_inputFormat ? m_inputFormat->gopSize() : 0;
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
        if(m_retries >= kRetryLimit)
        {
            NX_DEBUG(this) << m_url << "exceeded retry limit of " << kRetryLimit 
                << ". Error:" << ffmpeg::utils::errorToString(m_initCode);
            return;
        }

        waitForConsumers();
        if(m_terminated)
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
    bool needInit = m_cameraState == kModified || m_cameraState == kOff;
    if(m_cameraState == kModified)
    {   
        std::lock_guard<std::mutex>lock(m_mutex);
        uninitialize();
    }

    if(needInit)
    {
        std::lock_guard<std::mutex>lock(m_mutex);
        m_initCode = initialize();
        // if (m_initCode < 0) //todo inject the camera into the streamreader
        //     m_camera->setLastError(m_initCode);
    }

    return m_cameraState == kInitialized;
}

int VideoStream::initialize()
{
    int result = initializeInputFormat();
    if (result < 0)
        return result;

    result = initializeDecoder();
    if (result < 0)
        return result;

    m_cameraState = kInitialized;
    return 0;
}

void VideoStream::uninitialize()
{
    m_packetConsumerManager.consumerFlush();
    m_frameConsumerManager.consumerFlush();

    while (*m_packetCount > 0 || *m_frameCount > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

    if(m_decoder)
        m_decoder->flush();
    m_decoder.reset(nullptr);
    m_inputFormat.reset(nullptr);
    
    m_cameraState = kOff;
}

int VideoStream::initializeInputFormat()
{
    auto inputFormat = std::make_unique<ffmpeg::InputFormat>();

    int result = inputFormat->initialize(deviceType());
    if (result < 0)
        return result;

    setInputFormatOptions(inputFormat);

    result = inputFormat->open(ffmpegUrl().c_str());
    if (result < 0)
        return result;

    m_inputFormat = std::move(inputFormat);
    return 0;
}

void VideoStream::setInputFormatOptions(std::unique_ptr<ffmpeg::InputFormat>& inputFormat)
{
    AVFormatContext * context = inputFormat->formatContext();
    if(m_codecParams.codecID != AV_CODEC_ID_NONE)
        context->video_codec_id = m_codecParams.codecID;
    
    context->flags |= AVFMT_FLAG_DISCARD_CORRUPT | AVFMT_FLAG_NOBUFFER;

    if(m_codecParams.fps > 0)
        inputFormat->setFps(m_codecParams.fps);

    if(m_codecParams.width * m_codecParams.height > 0)
        inputFormat->setResolution(m_codecParams.width, m_codecParams.height);

    if(m_codecParams.bitrate > 0)
    {
        /*ffmpeg doesn't have an option for setting the bitrate on AVFormatContext.*/
        setBitrate(
            m_url.c_str(),
            m_codecParams.bitrate, 
            ffmpeg::utils::toNxCompressionType(m_codecParams.codecID));
    }
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
    int decodeCode = decode(packet, frame.get());
    if (decodeCode < 0)
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
    while(!gotFrame)
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

} // namespace usb_cam
} // namespace nx