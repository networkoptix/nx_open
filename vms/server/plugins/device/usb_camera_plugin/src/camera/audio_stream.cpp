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
static constexpr int kResyncThresholdMsec = 1000;

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
    m_packetConsumerManager(packetConsumerManager)
{
    if (!m_packetConsumerManager->empty())
        tryToStartIfNotStarted();
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
    m_wait.notify_all();

    tryToStartIfNotStarted();
}

void AudioStream::AudioStreamPrivate::removePacketConsumer(
    const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_packetConsumerManager->removeConsumer(consumer);
    }
    m_wait.notify_all();
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

bool AudioStream::AudioStreamPrivate::waitForConsumers()
{
    bool wait;
    {
        std::lock_guard<std::mutex> lockGuard(m_mutex);
        wait = m_packetConsumerManager->empty();
    }
    if (wait)
        uninitialize(); //< Don't stream the camera if there are no consumers.
    
    // Check again if there are no consumers. They could have been added during uninitialize().
    std::unique_lock<std::mutex> lock(m_mutex);
    if(m_packetConsumerManager->empty())
    {
        m_wait.wait(
            lock, 
            [&]() { return m_terminated || !m_packetConsumerManager->empty(); });
        return !m_terminated;
    }

    return true;
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
        checkIoError(m_initCode);
        if(m_initCode < 0)
        {
            setLastError(m_initCode);
            if (m_ioError)
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

#ifdef _WIN32
    // Decrease audio latency by reducing audio buffer size.
    static constexpr char kAudioBufferSizeKey[] = "audio_buffer_size";
    static constexpr int64_t kAudioBufferSizeValue = 80; // < milliseconds
    inputFormat->setEntry(kAudioBufferSizeKey, kAudioBufferSizeValue);

    // Dshow audio input format does not recognize io errors, so we set this flag to treat a
    // long timeout as an io error.
    inputFormat->formatContext()->flags |= AVFMT_FLAG_NONBLOCK;
#endif // _WIN32

    result = inputFormat->open(ffmpegUrlPlatformDependent().c_str());
    
    if (result < 0)
        return result;

    inputFormat->dumpFormat();

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

    int result = resampledFrame->getBuffer(
        context->sample_fmt,
        context->frame_size,
        context->channel_layout);
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
        frame->sampleFormat(),
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
        ffmpeg::Packet packet(m_inputFormat->audioCodecId(), AVMEDIA_TYPE_AUDIO);

        // AVFMT_FLAG_NONBLOCK is set if running on Windows. see initializeInputFormat().
        int result;
        if (m_inputFormat->formatContext()->flags & AVFMT_FLAG_NONBLOCK)
        {
            static constexpr std::chrono::milliseconds kTimeout = 
                std::chrono::milliseconds(1000);
            result = m_inputFormat->readFrameNonBlock(packet.packet(), kTimeout);
            if (result < AVERROR(EAGAIN)) //< Treat a timeout as an error.
                result = AVERROR(EIO);
        }
        else
        {
            result = m_inputFormat->readFrame(packet.packet());
        }

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
        AVMEDIA_TYPE_AUDIO);

    for(;;)
    {
        // need to drain the the resampler periodically to avoid increasing audio delay
        if (m_resampleContext && resampleDelay() > timePerVideoFrame())
        {
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
        }

        *outFfmpegError = encode(m_resampledFrame.get(), packet.get());
        if (*outFfmpegError == 0)
            break;
    }

    // Get duration before injecting into the packet because injection erases other fields.
    int64_t duration = packet->packet()->duration;

    // Inject ADTS header into each packet becuase AAC encoder does not do it.
    if (m_adtsInjector.inject(packet.get()) < 0)
        return nullptr;

    packet->setTimestamp(calculateTimestamp(duration));

    return packet;
}

uint64_t AudioStream::AudioStreamPrivate::calculateTimestamp(int64_t duration)
{
    uint64_t now = m_timeProvider->millisSinceEpoch();

    const AVRational sourceRate = { 1, m_encoder->sampleRate() };
    static const AVRational kTargetRate = { 1, 1000 }; // < One millisecond
    int64_t offsetMsec = av_rescale_q(m_offsetTicks, sourceRate, kTargetRate); 

    if (labs(now - m_baseTimestamp - offsetMsec) > kResyncThresholdMsec)
    {
        m_offsetTicks = 0;
        offsetMsec = 0;
        m_baseTimestamp = now;
    }

    uint64_t timestamp = m_baseTimestamp + offsetMsec;
    m_offsetTicks += duration;

    return timestamp;
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

void AudioStream::AudioStreamPrivate::terminate()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_terminated = true;
    m_wait.notify_all();
}

void AudioStream::AudioStreamPrivate::tryToStartIfNotStarted()
{
    std::lock_guard<std::mutex> lock(m_threadStartMutex);
    if (m_terminated)
    {
        if (pluggedIn())
            start();
    }
}

void AudioStream::AudioStreamPrivate::start()
{
    if (m_audioThread.joinable())
        m_audioThread.join();

    m_terminated = false;
    m_audioThread = std::thread(&AudioStream::AudioStreamPrivate::run, this);
}

void AudioStream::AudioStreamPrivate::stop()
{
    terminate();

    if (m_audioThread.joinable())
        m_audioThread.join();
}

void AudioStream::AudioStreamPrivate::run()
{
    while (!m_terminated)
    {
        if (!waitForConsumers())
            continue;

        if (!ensureInitialized())
            continue;

        int result = 0;
        auto packet = nextPacket(&result);
        checkIoError(result); // < Always check if it was an io error to reset m_ioError to false.
        if (result < 0)
        {
            setLastError(result);
            if (m_ioError)
                terminate();
            continue;
        }

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
    const std::string& url,
    const std::weak_ptr<Camera>& camera,
    bool enabled) 
    :
    m_url(url),
    m_camera(camera),
    m_packetConsumerManager(new PacketConsumerManager())
{
    setEnabled(enabled);
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