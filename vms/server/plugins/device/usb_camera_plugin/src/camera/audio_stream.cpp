#include "audio_stream.h"

#include <algorithm>

extern "C" {
#include <libswresample/swresample.h>
} // extern "C"

#include <plugins/plugin_container_api.h>

#include "camera.h"
#include "default_audio_encoder.h"
#include "ffmpeg/utils.h"
#include "device/audio/utils.h"

namespace nx {
namespace usb_cam {

namespace {

static constexpr int kMsecInSec = 1000;

static const char * ffmpegDeviceTypePlatformDependent()
{
#ifdef _WIN32
    return "dshow";
#elif __linux__
    return "alsa";
#endif
}

} // namespace


//-------------------------------------------------------------------------------------------------
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
    if (!m_packetConsumerManager->empty())
        tryStart();
}

AudioStream::AudioStreamPrivate::~AudioStreamPrivate()
{
    stop();
    m_timeProvider->releaseRef();
}

bool AudioStream::AudioStreamPrivate::pluggedIn() const
{
    return device::audio::pluggedIn(m_url);
}

bool AudioStream::AudioStreamPrivate::ioError() const
{
    return m_ioError;
}

void AudioStream::AudioStreamPrivate::addPacketConsumer(
    const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_packetConsumerManager->addConsumer(consumer);
    }

    tryStart();
}

void AudioStream::AudioStreamPrivate::removePacketConsumer(
    const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    bool shouldStop;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_packetConsumerManager->removeConsumer(consumer);
        shouldStop = m_packetConsumerManager->empty();
    }

    if (shouldStop)
        stop();
}

std::string AudioStream::AudioStreamPrivate::ffmpegUrlPlatformDependent() const
{
#ifdef _WIN32
    static const std::string kWindowsAudioPrefix = "audio=";
    
    // ffmpeg replaces all ":" with "_" in the audio devices alternative name.
    // It expects the modified alternative name or it will not open.
    static constexpr const char kWindowsAudioDelimitter = ':';
    static constexpr const char kFfmpegAudioDelimitter = '_';

    std::string url = m_url;
    std::replace(url.begin(), url.end(), kWindowsAudioDelimitter, kFfmpegAudioDelimitter);

    return kWindowsAudioPrefix + url;
#else
    return m_url;
#endif
}

int AudioStream::AudioStreamPrivate::initialize()
{
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

    m_initCode = m_adtsInjector.initialize(m_encoder.get());
    if (m_initCode < 0)
        return m_initCode;

    m_decodedFrame = std::make_unique<ffmpeg::Frame>();
    m_initialized = true;

    return 0;
}

void AudioStream::AudioStreamPrivate::uninitialize()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_packetConsumerManager->flush();
    }

    m_packetMergeBuffer.clear();

    while (*m_packetCount > 0)
    {
        static constexpr std::chrono::milliseconds kSleep(30);
        std::this_thread::sleep_for(kSleep);
    }

    if (m_decoder)
        m_decoder->flush();
    m_decoder.reset(nullptr);

    if (m_resampleContext)
        swr_free(&m_resampleContext);

    if (m_encoder)
        m_encoder->flush();
    m_encoder.reset(nullptr);

    m_adtsInjector.uninitialize();

    m_inputFormat.reset(nullptr);

    m_initialized = false;
}

bool AudioStream::AudioStreamPrivate::ensureInitialized()
{
    if(!m_initialized)
    {
        m_initCode = initialize();
        if(m_initCode < 0)
        {
            setLastError(m_initCode);
            if (checkIoError(m_initCode))
                terminate();
        }
    }
    return m_initialized;
}

int AudioStream::AudioStreamPrivate::initializeInputFormat()
{
    auto inputFormat = std::make_unique<ffmpeg::InputFormat>();
    int result = inputFormat->initialize(ffmpegDeviceTypePlatformDependent());
    if (result < 0)
        return result;
    
#ifdef WIN32
    // Decrease audio latency by reducing audio buffer size
    inputFormat->setEntry("audio_buffer_size", (int64_t)80); //< 80 milliseconds
#endif

    result = inputFormat->open(ffmpegUrlPlatformDependent().c_str());
    if (result < 0)
        return result;

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

    static constexpr int kDefaultFrameSize = 2000;

    int nbSamples = context->frame_size ? context->frame_size : kDefaultFrameSize;

    static constexpr int kDefaultAlignment =  32;

    int result = resampledFrame->getBuffer(
        context->sample_fmt,
        nbSamples,
        context->channel_layout,
        kDefaultAlignment);
    if (result < 0)
        return result;

    resampledFrame->frame()->sample_rate = context->sample_rate;

    m_resampledFrame = std::move(resampledFrame);

    return result;
}

int AudioStream::AudioStreamPrivate::initalizeResampleContext(const ffmpeg::Frame * frame)
{
    AVCodecContext * encoder = m_encoder->codecContext();

    m_resampleContext = swr_alloc_set_opts(
        nullptr,
        encoder->channel_layout,
        encoder->sample_fmt,
        encoder->sample_rate,
        frame->channelLayout(),
        (AVSampleFormat)frame->sampleFormat(),
        frame->sampleRate(),
        0,
        nullptr);

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
        ffmpeg::Packet packet(m_inputFormat->audioCodecID(), AVMEDIA_TYPE_AUDIO);
        int result = m_inputFormat->readFrame(packet.packet());
        if (result < 0)
            return result;

        result = m_decoder->sendPacket(packet.packet());
        if (result < 0 && result != AVERROR(EAGAIN))
            return result;

        result = m_decoder->receiveFrame(outFrame->frame());
        if (result == 0)
            return result;
        else if (result < 0 && result != AVERROR(EAGAIN))
            return result;
    }
}

int AudioStream::AudioStreamPrivate::resample(
    const ffmpeg::Frame * frame,
    ffmpeg::Frame * outFrame)
{
    if(!m_resampleContext)
    {
        m_initCode = initalizeResampleContext(frame);
        if(m_initCode < 0)
        {
            terminate();
            setLastError(m_initCode);
            return m_initCode;
        }
    }

    if(!m_resampleContext)
        return m_initCode;

    return swr_convert_frame(
        m_resampleContext,
        outFrame->frame(),
        frame ? frame->frame() : nullptr);
}

std::chrono::milliseconds AudioStream::AudioStreamPrivate::resampleDelay() const
{
    return m_resampleContext 
        ? std::chrono::milliseconds(swr_get_delay(m_resampleContext, kMsecInSec))
        : std::chrono::milliseconds(0);
}

int AudioStream::AudioStreamPrivate::encode(const ffmpeg::Frame* frame, ffmpeg::Packet * outPacket)
{
    int result = m_encoder->sendFrame(frame->frame());
    if (result < 0 && result != AVERROR(EAGAIN))
        return result;

    return m_encoder->receivePacket(outPacket->packet());
}

std::shared_ptr<ffmpeg::Packet> AudioStream::AudioStreamPrivate::nextPacket(int * outFfmpegError)
{
    auto packet = std::make_shared<ffmpeg::Packet>(
        m_encoder->codecId(),
        AVMEDIA_TYPE_AUDIO,
        m_packetCount);

    for(;;)
    {
        // need to drain the the resampler periodically to avoid increasing audio delay
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
                return nullptr;

            m_resampledFrame->frame()->pts = m_decodedFrame->pts();
            m_resampledFrame->frame()->pkt_pts = m_decodedFrame->packetPts();
        }

        *outFfmpegError = encode(m_resampledFrame.get(), packet.get());
        if (*outFfmpegError == 0)
            break;
    }

    //TODO: use AACPermanently in default_audio_encoder.cpp after archiving issues are fixed.
    if(packet->codecId() == AV_CODEC_ID_AAC)
    {
        // Inject ADTS header into each packet becuase AAC encoder does not do it.
        if (m_adtsInjector.inject(packet.get()) < 0)
            return nullptr;

        // AAC audio encoder puts out many small packets, causing choppy audio on the client side.
        // Merging multiple smaller packets into one larger one fixes the issue.
        if (auto pkt = mergePackets(packet, outFfmpegError))
            return pkt;
    }

    return packet;
}

void AudioStream::AudioStreamPrivate::terminate()
{
    m_terminated = true;
}

std::shared_ptr<ffmpeg::Packet> AudioStream::AudioStreamPrivate::mergePackets(
    const std::shared_ptr<ffmpeg::Packet>& packet,
    int * outFfmpegError)
{
    *outFfmpegError = 0;

    packet->setTimestamp(m_timeProvider->millisSinceEpoch());

    // Only merge if the timestamp difference is >= the amount of time it takes to produce a 
    // video frame.
    m_packetMergeBuffer.push_back(packet);
    if (packet->timestamp() - m_packetMergeBuffer[0]->timestamp() < timePerVideoFrame().count())
        return nullptr;

    // Do the merge now
    int size = 0;
    for (const auto& pkt : m_packetMergeBuffer)
        size += pkt->size();

    auto newPacket = std::make_shared<ffmpeg::Packet>(
        packet->codecId(),
        packet->mediaType());

    *outFfmpegError = newPacket->newPacket(size);
    if (*outFfmpegError < 0)
        return nullptr;
       
    uint8_t * data = newPacket->data();
    for (const auto & pkt : m_packetMergeBuffer)
    {
        memcpy(data, pkt->data(), pkt->size());
        data += pkt->size();
    }
    
    newPacket->setTimestamp(packet->timestamp());
    m_packetMergeBuffer.clear();

    return newPacket;
}

std::chrono::milliseconds AudioStream::AudioStreamPrivate::timePerVideoFrame() const
{
    if(auto cam = m_camera.lock())
        return cam->videoStream()->actualTimePerFrame();
    return std::chrono::milliseconds(0); //< Should never happen
}

bool AudioStream::AudioStreamPrivate::checkIoError(int ffmpegError)
{
    m_ioError = ffmpegError == AVERROR(EIO) //< Windows and Linux.
        || ffmpegError == AVERROR(ENODEV) //< Catch all.
        || ffmpegError == AVERROR(EBUSY); //< Device is busy during initialization.
    return m_ioError;
}

void AudioStream::AudioStreamPrivate::setLastError(int ffmpegError)
{
    if (auto cam = m_camera.lock())
        cam->setLastError(ffmpegError);
}

void AudioStream::AudioStreamPrivate::tryStart()
{
    if (m_terminated)
    {
        if (pluggedIn())
            start();
    }
}

void AudioStream::AudioStreamPrivate::start()
{
    if (m_terminated)
    {
        if (m_runThread.joinable())
            m_runThread.join();

        m_terminated = false;
        m_runThread = std::thread(&AudioStream::AudioStreamPrivate::run, this);
    }
}

void AudioStream::AudioStreamPrivate::stop()
{
    if (!m_terminated)
    {
        terminate();
        if (m_runThread.joinable())
            m_runThread.join();
    }
}

void AudioStream::AudioStreamPrivate::run()
{
    while (!m_terminated)
    {
        if (!ensureInitialized())
            continue;

        int result = 0;
        auto packet = nextPacket(&result);
        if (result < 0)
        {
            setLastError(result);
            if (checkIoError(result))
                terminate();
            continue;
        }

        // If the encoder is AAC, some packets are buffered and copied before delivering.
        // In that case, packet is nullptr.
        if (packet)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_packetConsumerManager->givePacket(packet);
        }
    }

    uninitialize();
}

//-------------------------------------------------------------------------------------------------
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
    {
        m_streamReader.reset(nullptr);
    }
}

bool AudioStream::enabled() const
{
    return m_streamReader != nullptr;
}

bool AudioStream::ioError() const
{
    return m_streamReader ? m_streamReader->ioError() : false;
}

void AudioStream::addPacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    if (m_streamReader)
        m_streamReader->addPacketConsumer(consumer);
    else
        m_packetConsumerManager->addConsumer(consumer);
}

void AudioStream::removePacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    if (m_streamReader)
        m_streamReader->removePacketConsumer(consumer);
    else
        m_packetConsumerManager->removeConsumer(consumer);
}

} //namespace usb_cam
} //namespace nx 