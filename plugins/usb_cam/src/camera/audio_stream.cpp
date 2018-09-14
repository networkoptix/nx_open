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

int constexpr kRetryLimit = 10;
constexpr int kMsecInSec = 100;

const char * ffmpegDeviceType()
{
#ifdef _WIN32
    return "dshow";
#elif __linux__
    return "alsa";
#endif
}

} // namespace

/////////////////////////////////////// AudioStreamPrivate /////////////////////////////////////////

AudioStream::AudioStreamPrivate::AudioStreamPrivate(
    const std::string& url,
    const std::weak_ptr<Camera>& camera,
    const std::shared_ptr<PacketConsumerManager>& packetConsumerManager)
    :
    m_url(url),
    m_camera(camera),
    m_timeProvider(camera.lock()->timeProvider()),
    m_packetConsumerManager(packetConsumerManager),
    m_initialized(false),
    m_initCode(0),
    m_retries(0),
    m_terminated(false),
    m_resampleContext(nullptr),
    m_packetCount(std::make_shared<std::atomic_int>()),
    m_bufferMaxSize(3)
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
    m_packetBuffer.clear();

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

int AudioStream::AudioStreamPrivate::decodeNextFrame(AVFrame * outFrame)
{
    int result = 0;
    for(;;)
    {
        result = m_decoder->receiveFrame(outFrame);
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

    return result;
}

int AudioStream::AudioStreamPrivate::resample(const AVFrame * frame, AVFrame * outFrame)
{
    if(!m_resampleContext)
    {
        m_initCode = initalizeResampleContext(frame);
        if(m_initCode < 0)
            return m_initCode;
    }

    if(!m_resampleContext)
        return m_initCode;

    return swr_convert_frame(m_resampleContext, outFrame, frame);
}

std::shared_ptr<ffmpeg::Packet> AudioStream::AudioStreamPrivate::getNextData(int * outError)
{
    #define returnData(res, retObj) do{ *outError = res; return retObj; }while(0)

    int result = 0;
    auto packet = std::make_shared<ffmpeg::Packet>(
        m_encoder->codecID(),
        AVMEDIA_TYPE_AUDIO,
        m_packetCount);
    for(;;)
    {
        result = m_encoder->receivePacket(packet->packet());
        if (result == 0)
            break;
        else if (result < 0 && result != AVERROR(EAGAIN))
            returnData(result, nullptr);

        if(m_resampleContext && swr_get_delay(m_resampleContext, kMsecInSec) > timePerVideoFrame())
        {
            result = resample(nullptr, m_resampledFrame->frame());
            if (result < 0)
                continue;
        }
        else
        {
            result = decodeNextFrame(m_decodedFrame->frame());
            if (result < 0)
                returnData(result, nullptr);

            result = resample(m_decodedFrame->frame(), m_resampledFrame->frame());
            if(result < 0)
            {
                ++m_retries;
                returnData(result, nullptr);
            }
        }

        result = m_encoder->sendFrame(m_resampledFrame->frame());
        if (result < 0 && result != AVERROR(EAGAIN))
            returnData(result, nullptr);
    }
    
    if (auto pkt = addToBuffer(packet, &result))
        returnData(result, pkt);

    returnData(result, nullptr);
}

/**
* AAC audio encoder puts out too many packets, bogging down the fps. Providing less packets fixes
* the issue, but we don't want to lose any, so copy multiple packets into one and then provide 
* just the one.
*/
std::shared_ptr<ffmpeg::Packet> AudioStream::AudioStreamPrivate::addToBuffer(
    const std::shared_ptr<ffmpeg::Packet>& packet,
    int * outFfmpegError)
{
    *outFfmpegError = 0;

    if(m_encoder->codecID() == AV_CODEC_ID_PCM_S16LE)
    {
        packet->setTimeStamp(m_timeProvider->millisSinceEpoch());
        return packet;
    }

    m_packetBuffer.push_back(packet);
    packet->setTimeStamp(m_timeProvider->millisSinceEpoch());
    if (packet->timeStamp() - m_packetBuffer[0]->timeStamp() < timePerVideoFrame())
        return nullptr;

    int size = 0;
    for (const auto& pkt : m_packetBuffer)
        size += pkt->size();

    auto newPacket = std::make_shared<ffmpeg::Packet>(
        m_packetBuffer[0]->codecID(),
        AVMEDIA_TYPE_AUDIO);

    *outFfmpegError = newPacket->newPacket(size);
    if (*outFfmpegError < 0)
        return nullptr;
       
    uint8_t * data = newPacket->data();
    for (const auto & pkt : m_packetBuffer)
    {
        memcpy(data, pkt->data(), pkt->size());
        data += pkt->size();
    }

    newPacket->setTimeStamp(packet->timeStamp());
    m_packetBuffer.clear();
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

        int result;
        auto packet = getNextData(&result);
        if (result < 0)
        {
            // EIO is returned when the device is unplugged for audio
            m_terminated = result == AVERROR(EIO);
            continue;
        }
        if(!packet)
            continue;

        std::lock_guard<std::mutex> lock(m_mutex);
        m_packetConsumerManager->givePacket(packet);
    }
}

int AudioStream::AudioStreamPrivate::timePerVideoFrame() const
{
    float fps = 30;
    if (auto cam = m_camera.lock())
        fps = cam->videoStream()->fps();
    return 1.0 / fps * kMsecInSec;
}


/////////////////////////////////////////// AudioStream ////////////////////////////////////////////


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