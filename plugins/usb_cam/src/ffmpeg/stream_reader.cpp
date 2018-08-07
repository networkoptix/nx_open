#include "stream_reader.h"

#include <algorithm>
#ifdef _WIN32
#elif __linux__
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#endif

#include <nx/utils/log/log.h>
#include <nx/utils/app_info.h>
#include <plugins/plugin_container_api.h>

#include "buffered_stream_consumer.h"
#include "device/utils.h"
#include "utils.h"
#include "input_format.h"
#include "codec.h"
#include "packet.h"

namespace nx {
namespace ffmpeg {

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

const int RETRY_LIMIT = 10;

} // namespace

StreamReader::StreamReader(
    const char * url,
    const CodecParameters& codecParams,
    nxpl::TimeProvider *const timeProvider)
    :
    m_url(url),
    m_codecParams(codecParams),
    m_timeProvider(timeProvider),
    m_cameraState(kOff),
    m_lastFfmpegError(0),
    m_packetCount(std::make_shared<std::atomic_int>(0)),
    m_frameCount(std::make_shared<std::atomic_int>(0)),
    m_terminated(false),
    m_retries(0)
{
    start();
}

StreamReader::~StreamReader()
{
    stop();
    uninitialize();
}

void StreamReader::addPacketConsumer(const std::weak_ptr<PacketConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (packetConsumerIndex(consumer) >= m_packetConsumers.size())
    {
        m_packetConsumers.push_back(consumer);
        updateUnlocked();
        m_wait.notify_all();
    }
}

void StreamReader::removePacketConsumer(const std::weak_ptr<PacketConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    int index = packetConsumerIndex(consumer);
    if(index < m_packetConsumers.size())
        m_packetConsumers.erase(m_packetConsumers.begin() + index);

    updateUnlocked();
}

void StreamReader::addFrameConsumer(const std::weak_ptr<FrameConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (frameConsumerIndex(consumer) >= m_frameConsumers.size())
    {
        m_frameConsumers.push_back(consumer);
        updateUnlocked();
        m_wait.notify_all();
    }
}

void StreamReader::removeFrameConsumer(const std::weak_ptr<FrameConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    int index = frameConsumerIndex(consumer);
    if(index < m_frameConsumers.size())
        m_frameConsumers.erase(m_frameConsumers.begin() + index);

    updateUnlocked();
}

std::string StreamReader::ffmpegUrl() const
{
    return
#ifdef _WIN32
        std::string("video=@device_pnp_") + m_url;
#else
        m_url;
#endif
}

void StreamReader::updateFpsUnlocked()
{
    if(m_packetConsumers.empty())
        return;

    float largest = 0;
    for (const auto & consumer : m_packetConsumers)
    {
        if (auto c = consumer.lock())
        {
            float fps = c->fps();
            if (fps > largest)
                largest = fps;
        }
    }

    for (const auto & consumer : m_frameConsumers)
    {
        if (auto c = consumer.lock())
        {
            float fps = c->fps();
            if (fps > largest)
                largest = fps;
        }
    }
    
    if (m_codecParams.fps != largest)
    {
        m_codecParams.fps = largest;
        m_cameraState = kModified;
    }
}

void StreamReader::updateResolutionUnlocked()
{
    if (m_packetConsumers.empty())
        return;

    int largestWidth = 0;
    int largestHeight = 0;
    for (const auto & consumer : m_packetConsumers)
    {
        if (auto c = consumer.lock())
        {
            int width;
            int height;
            c->resolution(&width, &height);
            if (width * height > largestWidth * largestHeight)
            {
                largestWidth = width;
                largestHeight = height;
            }
        }
    }

    for (const auto & consumer : m_frameConsumers)
    {
        if (auto c = consumer.lock())
        {
            int width;
            int height;
            c->resolution(&width, &height);
            if (width * height > largestWidth * largestHeight)
            {
                largestWidth = width;
                largestHeight = height;
            }
        }
    }

    if(m_codecParams.width * m_codecParams.height != largestWidth * largestHeight)
    {
        m_codecParams.setResolution(largestWidth, largestHeight);
        m_cameraState = kModified;
    }
}

void StreamReader::updateBitrateUnlocked()
{
    if (m_packetConsumers.empty())
        return;

    int largest = 0;
    for (const auto & consumer : m_packetConsumers)
    {
        if (auto c = consumer.lock())
        {
            int bitrate = c->bitrate();
            if (bitrate > largest)
                largest = bitrate;
        }
    }

    for (const auto & consumer : m_frameConsumers)
    {
        if (auto c = consumer.lock())
        {
            int bitrate = c->bitrate();
            if (bitrate > largest)
                largest = bitrate;
        }
    }

    if (largest != m_codecParams.bitrate)
    {
        m_codecParams.bitrate = largest;
        m_cameraState = kModified;
    }
}

void StreamReader::updateUnlocked()
{
    updateFpsUnlocked();
    updateResolutionUnlocked();
    updateBitrateUnlocked();
    NX_DEBUG(this) << m_url + ":" << "Selected Params:" << m_codecParams.toString();
}

int StreamReader::packetConsumerIndex (const std::weak_ptr<PacketConsumer> &consumer)
{
    int index = 0;
    for (const auto& c : m_packetConsumers)
    {
        if (c.lock() == consumer.lock())
            return index;
        ++index;
    }
    return index;
}

int StreamReader::frameConsumerIndex (const std::weak_ptr<FrameConsumer> &consumer)
{
    int index = 0;
    for (const auto& c : m_frameConsumers)
    {
        if (c.lock() == consumer.lock())
            return index;
        ++index;
    }
    return index;
}

void StreamReader::start()
{
    m_terminated = false;
    m_runThread = std::thread(&StreamReader::run, this);
}

void StreamReader::stop()
{
    m_terminated = true;
    m_wait.notify_all();
    if(m_runThread.joinable())
        m_runThread.join();
}

AVPixelFormat StreamReader::decoderPixelFormat() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_decoder ? m_decoder->pixelFormat() : AV_PIX_FMT_NONE;
}

int StreamReader::gopSize() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_inputFormat ? m_inputFormat->gopSize() : 0;
}

int StreamReader::fps() const
{
    return m_codecParams.fps;
}

void StreamReader::updateFps()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    updateFpsUnlocked();
}

void StreamReader::updateBitrate()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    updateBitrateUnlocked();
}

void StreamReader::updateResolution()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    updateResolutionUnlocked();
}

std::string StreamReader::url() const
{
    return m_url;
}

int StreamReader::lastFfmpegError() const
{
    return m_lastFfmpegError;
}

void StreamReader::run()
{
    while (!m_terminated)
    {
        if (!ensureInitialized())
        {
            if (m_retries++ >= RETRY_LIMIT)
                return;
        }

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_packetConsumers.empty() && m_frameConsumers.empty())
                m_wait.wait(lock, 
                    [&]()
                    {
                        return m_terminated 
                            || !m_packetConsumers.empty() 
                            || !m_frameConsumers.empty(); 
                    });
        }

        if(m_terminated)
             return;

        if(!m_inputFormat)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
        }

        auto packet = std::make_shared<Packet>(m_inputFormat->videoCodecID(), m_packetCount);
        int readCode = m_inputFormat->readFrame(packet->packet());
        if (readCode < 0)
            continue;

        packet->setTimeStamp(m_timeProvider->millisSinceEpoch());
        m_timeStamps.insert(std::pair<int64_t, int64_t>(packet->pts(), packet->timeStamp()));

        auto frame = std::make_shared<Frame>(m_frameCount);
        int decodeCode = decode(packet.get(), frame.get());
        if (decodeCode < 0)
            continue;

        auto it = m_timeStamps.find(frame->pts());
        if(it != m_timeStamps.end())
        {
            frame->setTimeStamp(it->second);
            m_timeStamps.erase(it);
        }
        else
        {
            frame->setTimeStamp(m_timeProvider->millisSinceEpoch());
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto & consumer : m_packetConsumers)
        {
            if (auto c = consumer.lock())
                c->givePacket(packet);
        }

        for (auto & consumer : m_frameConsumers)
        {
            if (auto c = consumer.lock())
                c->giveFrame(frame);
        }
    }
}

bool StreamReader::ensureInitialized()
{
    switch(m_cameraState)
    {
        case kModified:
            uninitialize();
            initialize();
            break;
        case kOff:
            initialize();
            break;
    }

    return m_cameraState == kInitialized;
}

int StreamReader::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    int initCode = initializeInputFormat();
    if (initCode < 0)
        return initCode;

    initCode = initializeDecoder();
    if (initCode < 0)
        return initCode;

    m_cameraState = kInitialized;

    return 0;
}

void StreamReader::uninitialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto & consumer : m_packetConsumers)
    {
        if (auto c = consumer.lock())
            c->clear();
    }

    for (auto & consumer : m_frameConsumers)
    {
        if (auto c = consumer.lock())
            c->clear();
    }

    while (*m_packetCount > 0 || *m_frameCount > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        flush();
        m_decoder.reset(nullptr);
        m_inputFormat.reset(nullptr);
        m_cameraState = kOff;
}

int StreamReader::initializeInputFormat()
{
    auto inputFormat = std::make_unique<InputFormat>();

    int initCode = inputFormat->initialize(deviceType());
    if (initCode < 0)
    {
        NX_DEBUG(this) << "InputFormat init failed:" << utils::errorToString(initCode);
        m_lastFfmpegError = initCode;
        return initCode;
    }

    setInputFormatOptions(inputFormat);

    int openCode = inputFormat->open(ffmpegUrl().c_str());
    if (openCode < 0)
        return openCode;

    m_inputFormat = std::move(inputFormat);
    return 0;
}

void StreamReader::setInputFormatOptions(const std::unique_ptr<InputFormat>& inputFormat)
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
        /*ffmpeg doesn't have an option for setting the bitrate.*/
        setBitrate(
            m_url.c_str(),
            m_codecParams.bitrate, 
            utils::toNxCompressionType(m_codecParams.codecID));
    }
}

int StreamReader::initializeDecoder()
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

    m_decoder = std::move(decoder);
    return 0;
}

int StreamReader::decode(const Packet * packet, Frame * frame)
{
    int returnCode = 0;
    bool gotFrame = false;
    while(!gotFrame)
    {
        returnCode = m_decoder->sendPacket(packet->packet());
        bool needReceive = returnCode == 0 || returnCode == AVERROR(EAGAIN);
        while (needReceive) // ready to or requires parsing a frame
        {
            returnCode = m_decoder->receiveFrame(frame->frame());
            if (returnCode == 0 || returnCode == AVERROR(EAGAIN)) 
            // got a frame or need to send more packets to decoder
            {
                gotFrame = returnCode == 0;
                break;
            }
            else // something very bad happened
            {
                return returnCode;
            }
        }
        if (!needReceive) // something very bad happened
        {
            return returnCode;
        }
    }

    return returnCode;
}

void StreamReader::flush()
{
    ffmpeg::Frame frame;
    int returnCode = m_decoder->sendPacket(nullptr);
    while(returnCode != AVERROR_EOF)
        returnCode = m_decoder->receiveFrame(frame.frame());
}

} // namespace ffmpeg
} // namespace nx