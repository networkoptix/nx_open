#include "audio_stream.h"

extern "C" {
#include <libswresample/swresample.h>
} // extern "C"

#include <nx/utils/log/log.h>
#include <plugins/plugin_container_api.h>

#include "camera.h"
#include "default_audio_encoder.h"
#include "ffmpeg/utils.h"

namespace nx {
namespace usb_cam {

namespace {

static constexpr int kRetryLimit = 10;
static constexpr int kMsecInSec = 1000;
static constexpr float kDefaultFps = 30;

const char * ffmpegDeviceType()
{
#ifdef _WIN32
    return "dshow";
#elif __linux__
    return "alsa";
#endif
}

} // namespace


//--------------------------------------------------------------------------------------------------
// AudioStreamPrivate

AudioStream::AudioStreamPrivate::AudioStreamPrivate(
    const std::string& url,
    const std::weak_ptr<Camera>& camera,
    const std::shared_ptr<PacketConsumerManager>& packetConsumerManager)
    :
    m_url(url),
    m_camera(camera),
    m_timeProvider(camera.lock()->timeProvider()),
    m_packetConsumerManager(packetConsumerManager),
    m_packetCount(std::make_shared<std::atomic_int>())
{
    start();
}

AudioStream::AudioStreamPrivate::~AudioStreamPrivate()
{
    stop();
    uninitialize();
    m_timeProvider->releaseRef();
}

void AudioStream::AudioStreamPrivate::addPacketConsumer(const std::weak_ptr<PacketConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_packetConsumerManager->addConsumer(consumer, false);
    m_wait.notify_all();
}

void AudioStream::AudioStreamPrivate::removePacketConsumer(const std::weak_ptr<PacketConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_packetConsumerManager->removeConsumer(consumer);
    m_wait.notify_all();
}

int AudioStream::AudioStreamPrivate::sampleRate() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_encoder ? m_encoder->codecContext()->sample_rate : 0;
}

int AudioStream::sampleRate() const
{
    return m_streamReader ? m_streamReader->sampleRate() : 0;
}

std::string AudioStream::AudioStreamPrivate::ffmpegUrl() const
{
#ifdef _WIN32
    return std::string("audio=") + m_url;
#else
    return m_url;
#endif
}

void AudioStream::AudioStreamPrivate::waitForConsumers()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_packetConsumerManager->empty())
    {
        uninitialize();
        m_wait.wait(lock, 
            [&]()
            {
                return m_terminated || !m_packetConsumerManager->empty(); 
            });
    }
}

int AudioStream::AudioStreamPrivate::initialize()
{
    m_initCode = 0;
    m_initCode = initializeInputFormat();
    if (m_initCode < 0) 
        return m_initCode;

    m_initCode = initializeDecoder();
    if (m_initCode < 0)
        return m_initCode;

    m_initCode = initializeEncoder();
    if (m_initCode < 0)
        return m_initCode;

    m_initCode = initializeResampledFrame();
    if (m_initCode < 0)
        return m_initCode;

    m_decodedFrame = std::make_unique<ffmpeg::Frame>();
    m_initialized = true;
    return 0;
}

void AudioStream::AudioStreamPrivate::uninitialize()
{
    m_packetConsumerManager->consumerFlush();
    m_packetMergeBuffer.clear();

    while(*m_packetCount > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

    if (m_decoder)
        m_decoder->flush();
    m_decoder.reset(nullptr);

    if (m_resampleContext)
        swr_free(&m_resampleContext);

    if (m_encoder)
        m_encoder->flush();
    m_encoder.reset(nullptr);

    m_inputFormat.reset(nullptr);
    m_initialized = false;
}

bool AudioStream::AudioStreamPrivate::ensureInitialized()
{
    if(!m_initialized)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_initCode = initialize();
        if (m_initCode < 0)
        {
            if (auto cam = m_camera.lock())
                cam->setLastError(m_initCode);
        }
    }
    return m_initialized;
}

int AudioStream::AudioStreamPrivate::initializeInputFormat()
{
    auto inputFormat = std::make_unique<ffmpeg::InputFormat>();
    int result = inputFormat->initialize(ffmpegDeviceType());
    if (result < 0)
        return result;
    
    result = inputFormat->open(ffmpegUrl().c_str());
    if (result < 0)
        return result;

#ifdef WIN32
    /**
     * Attempt to avoid "real-time buffer too full" error messages in Windows
     */
    inputFormat->setEntry("thread_queue_size", 2048);
    inputFormat->setEntry("threads", (int64_t) 0);
#endif

    m_inputFormat = std::move(inputFormat);
    return 0;
}

int AudioStream::AudioStreamPrivate::initializeDecoder()
{
    auto decoder = std::make_unique<ffmpeg::Codec>();
    AVStream * stream = m_inputFormat->findStream(AVMEDIA_TYPE_AUDIO);
    if (!stream)
        return AVERROR_DECODER_NOT_FOUND;

    int result = decoder->initializeDecoder(stream->codecpar);
    if (result < 0)
        return result;

    auto context = decoder->codecContext();
    context->request_channel_layout = ffmpeg::utils::suggestChannelLayout(decoder->codec());
    context->channel_layout = context->request_channel_layout;

    result = decoder->open();
    if (result < 0)
        return result;

    m_decoder = std::move(decoder);
    return 0;
}

int AudioStream::AudioStreamPrivate::initializeEncoder()
{
    int result = 0;
    auto encoder = getDefaultAudioEncoder(&result);
    if (result < 0)
        return result;

    result = encoder->open();
    if (result < 0)
        return result;

    m_encoder = std::move(encoder);
    return 0;
}

int AudioStream::AudioStreamPrivate::initializeResampledFrame()
{
    auto resampledFrame = std::make_unique<ffmpeg::Frame>();
    auto context = m_encoder->codecContext();

    int nbSamples = context->frame_size ? context->frame_size : 2000;

    int result = resampledFrame->getBuffer(
        context->sample_fmt,
        nbSamples,
        context->channel_layout,
        32);
    if (result < 0)
        return result;

    resampledFrame->frame()->sample_rate = context->sample_rate;

    m_resampledFrame = std::move(resampledFrame);
    return result;
}

int AudioStream::AudioStreamPrivate::initalizeResampleContext(const AVFrame * frame)
{
    AVCodecContext * encoder = m_encoder->codecContext();

    m_resampleContext = swr_alloc_set_opts(
        nullptr,
        encoder->channel_layout,
        encoder->sample_fmt,
        encoder->sample_rate,
        frame->channel_layout,
        (AVSampleFormat)frame->format,
        frame->sample_rate,
        0, nullptr);

    if (!m_resampleContext)
        return AVERROR(ENOMEM);

    int result = swr_init(m_resampleContext);
    if (result < 0)
        swr_free(&m_resampleContext);

    return result;
}

int AudioStream::AudioStreamPrivate::decodeNextFrame(ffmpeg::Frame * outFrame)
{
    for(;;)
    {
        int result = m_decoder->receiveFrame(outFrame->frame());
        if (result == 0)
            return result;
        else if (result < 0 && result != AVERROR(EAGAIN))
            return result;

        ffmpeg::Packet packet(m_inputFormat->audioCodecID(), AVMEDIA_TYPE_AUDIO);
        result = m_inputFormat->readFrame(packet.packet());
        if (result < 0)
            return result;

        result = m_decoder->sendPacket(packet.packet());
        if(result < 0 && result != AVERROR(EAGAIN))
            return result;
    }
}

int AudioStream::AudioStreamPrivate::resample(const ffmpeg::Frame * frame, ffmpeg::Frame * outFrame)
{
    if(!m_resampleContext)
    {
        m_initCode = initalizeResampleContext(frame->frame());
        if(m_initCode < 0)
        {
            if(auto cam = m_camera.lock())
                cam->setLastError(m_initCode);
            return m_initCode;
        }
    }

    if(!m_resampleContext)
        return m_initCode;

    return swr_convert_frame(m_resampleContext, outFrame->frame(), frame ? frame->frame() : nullptr);
}

std::chrono::milliseconds AudioStream::AudioStreamPrivate::resampleDelay() const
{
    return m_resampleContext 
        ? std::chrono::milliseconds(swr_get_delay(m_resampleContext, kMsecInSec))
        : std::chrono::milliseconds(0);
}

std::shared_ptr<ffmpeg::Packet> AudioStream::AudioStreamPrivate::nextPacket(int * outFfmpegError)
{
    auto packet = std::make_shared<ffmpeg::Packet>(
        m_encoder->codecID(),
        AVMEDIA_TYPE_AUDIO,
        m_packetCount);

    for(;;)
    {
        *outFfmpegError = m_encoder->receivePacket(packet->packet());
        if (*outFfmpegError == 0)
            break;
        else if (*outFfmpegError < 0 && *outFfmpegError != AVERROR(EAGAIN))
            return nullptr;

        // need to drain the the resampler to avoid increasing audio delay
        if(m_resampleContext && resampleDelay() > timePerVideoFrame())
        {
            m_resampledFrame->frame()->pts = AV_NOPTS_VALUE;
            m_resampledFrame->frame()->pkt_pts = AV_NOPTS_VALUE;
            *outFfmpegError = resample(nullptr, m_resampledFrame.get());
            if (*outFfmpegError < 0)
                continue;
        }
        else
        {
            *outFfmpegError = decodeNextFrame(m_decodedFrame.get());
            if (*outFfmpegError < 0)
                return nullptr;

            *outFfmpegError = resample(m_decodedFrame.get(), m_resampledFrame.get());
            if(*outFfmpegError < 0)
            {
                ++m_retries;
                return nullptr;
            }

            m_resampledFrame->frame()->pts = m_decodedFrame->pts();
            m_resampledFrame->frame()->pkt_pts = m_decodedFrame->packetPts();
        }

        *outFfmpegError = m_encoder->sendFrame(m_resampledFrame->frame());
        if (*outFfmpegError < 0 && *outFfmpegError != AVERROR(EAGAIN))
            return nullptr;
    }
    
    /**
    * AAC audio encoder puts out too many packets, bogging down the fps. Providing less packets 
    * fixes the issue, but we don't want to lose any, so merge multiple packets.
    */
    if (auto pkt = mergeAllPackets(packet, outFfmpegError))
        return pkt;

    return nullptr;
}

std::shared_ptr<ffmpeg::Packet> AudioStream::AudioStreamPrivate::mergeAllPackets(
    const std::shared_ptr<ffmpeg::Packet>& packet,
    int * outFfmpegError)
{
    *outFfmpegError = 0;

    packet->setTimeStamp(m_timeProvider->millisSinceEpoch());

    if(m_encoder->codecID() == AV_CODEC_ID_PCM_S16LE)
        return packet;

    /** 
     * Only merge if the timestamp difference is >= the amount of time it takes the video stream
     * to produce a video frame
     */
    m_packetMergeBuffer.push_back(packet);
    if (packet->timeStamp() - m_packetMergeBuffer[0]->timeStamp() < timePerVideoFrame().count())
        return nullptr;


    /** Do the merge now */

    int size = 0;
    for (const auto& pkt : m_packetMergeBuffer)
        size += pkt->size();

    auto newPacket = std::make_shared<ffmpeg::Packet>(
        packet->codecID(),
        AVMEDIA_TYPE_AUDIO);

    *outFfmpegError = newPacket->newPacket(size);
    if (*outFfmpegError < 0)
        return nullptr;
       
    uint8_t * data = newPacket->data();
    for (const auto & pkt : m_packetMergeBuffer)
    {
        memcpy(data, pkt->data(), pkt->size());
        data += pkt->size();
    }

    newPacket->setTimeStamp(m_packetMergeBuffer[0]->timeStamp());
    m_packetMergeBuffer.clear();

    return newPacket;
}

void AudioStream::AudioStreamPrivate::start()
{
    m_terminated = false;
    m_runThread = std::thread(&AudioStream::AudioStreamPrivate::run, this);
}

void AudioStream::AudioStreamPrivate::stop()
{
    m_terminated = true;
    m_wait.notify_all();
    if (m_runThread.joinable())
        m_runThread.join();
}

void AudioStream::AudioStreamPrivate::run()
{
    while (!m_terminated)
    {
        if (m_retries >= kRetryLimit)
        {
            NX_DEBUG(this) << "Exceeded the retry limit of " << kRetryLimit
                << "with error: " << ffmpeg::utils::errorToString(m_initCode);
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

        int result = 0;
        auto packet = nextPacket(&result);
        if (result < 0)
        {
            // EIO is returned when the device is unplugged for audio
            m_terminated = result == AVERROR(EIO);
            if(m_terminated)
            {
                if(auto cam = m_camera.lock())
                    cam->setLastError(result);
            }
            continue;
        }
        /**
         *  If the encoder is aac, some packets are buffered and copied before delivering.
         *  In that case, this packet is nullptr.
         */
        if(!packet)
            continue;

        std::lock_guard<std::mutex> lock(m_mutex);
        m_packetConsumerManager->givePacket(packet);
    }
}

std::chrono::milliseconds AudioStream::AudioStreamPrivate::timePerVideoFrame() const
{
    float fps = 0;
    if (auto cam = m_camera.lock())
        fps = cam->videoStream()->fps();

    /** should never happen */
    if (fps == 0)
        fps == kDefaultFps;

    return std::chrono::milliseconds((int)(1.0 / fps * kMsecInSec));
}


//--------------------------------------------------------------------------------------------------
// AudioStream

AudioStream::AudioStream(
    const std::string url,
    const std::weak_ptr<Camera>& camera,
    bool enabled) 
    :
    m_url(url),
    m_camera(camera),
    m_packetConsumerManager(new PacketConsumerManager())
{
    setEnabled(enabled);
}

AudioStream::~AudioStream()
{
}

std::string AudioStream::url() const
{
    return m_url;
}

void AudioStream::setEnabled(bool enabled)
{
    if (enabled)
    {
        if(!m_streamReader)
        {
            m_streamReader = std::make_unique<AudioStreamPrivate>(
                m_url,
                m_camera,
                m_packetConsumerManager);
        }
    }
    else
        m_streamReader.reset(nullptr);
}

bool AudioStream::enabled() const
{
    return m_streamReader != nullptr;
}

void AudioStream::addPacketConsumer(const std::weak_ptr<PacketConsumer>& consumer)
{
    if (m_streamReader)
        m_streamReader->addPacketConsumer(consumer);
    else
        m_packetConsumerManager->addConsumer(consumer);
}

void AudioStream::removePacketConsumer(const std::weak_ptr<PacketConsumer>& consumer)
{
    if (m_streamReader)
        m_streamReader->removePacketConsumer(consumer);
    else
        m_packetConsumerManager->removeConsumer(consumer);
}

} //namespace usb_cam
} //namespace nx 