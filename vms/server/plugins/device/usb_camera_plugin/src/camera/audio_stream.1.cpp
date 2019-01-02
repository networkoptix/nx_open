#include "audio_common.h"
#if defined(AUDIO_STREAM_1)

#include "audio_stream.1.h"

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

AudioStream1::AudioStreamPrivate::AudioStreamPrivate(
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
        tryToStartIfNotStarted();
}

AudioStream1::AudioStreamPrivate::~AudioStreamPrivate()
{
    stop();
    m_timeProvider->releaseRef();
}

bool AudioStream1::AudioStreamPrivate::pluggedIn() const
{
    return device::audio::pluggedIn(m_url);
}

bool AudioStream1::AudioStreamPrivate::ioError() const
{
    return m_ioError;
}

void AudioStream1::AudioStreamPrivate::addPacketConsumer(
    const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_packetConsumerManager->addConsumer(consumer);
    }
    m_wait.notify_all();

    tryToStartIfNotStarted();
}

void AudioStream1::AudioStreamPrivate::removePacketConsumer(
    const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_packetConsumerManager->removeConsumer(consumer);
    }
    m_wait.notify_all();
}

std::string AudioStream1::AudioStreamPrivate::ffmpegUrlPlatformDependent() const
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

bool AudioStream1::AudioStreamPrivate::noConsumers() const
{    
    std::lock_guard<std::mutex> lockGuard(m_mutex);
    return m_packetConsumerManager->empty();
}

std::shared_ptr<ffmpeg::Packet> AudioStream1::AudioStreamPrivate::popNextPacket()
{
    std::lock_guard<std::mutex> lock(m_resampleBufferMutex);

    if (m_resampleBuffer.empty())
        return nullptr;

    auto packet = m_resampleBuffer.front();
    m_resampleBuffer.pop_front();

    return packet;
}

void AudioStream1::AudioStreamPrivate::thisThreadSleep()
{
    static const std::chrono::milliseconds kSleep = std::chrono::milliseconds(1);
    std::this_thread::sleep_for(kSleep);
}

int AudioStream1::AudioStreamPrivate::initializeResampler()
{
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

void AudioStream1::AudioStreamPrivate::uninitialize()
{
    m_inputFormat.reset(nullptr);

    m_decoder.reset(nullptr);

    if (m_resampleContext)
        swr_free(&m_resampleContext);

    if (m_encoder)
        m_encoder->flush();
    m_encoder.reset(nullptr);

    m_adtsInjector.uninitialize();

    m_initialized = false;
}

bool AudioStream1::AudioStreamPrivate::ensureFormatInitialized()
{
    if(!m_inputFormat)
    {
        m_initCode = initializeInputFormat();
        checkIoError(m_initCode);
        if(m_initCode < 0)
        {
            setLastError(m_initCode);
            if (m_ioError)
                terminate();
        }
    }
    return m_inputFormat != nullptr;
}

bool AudioStream1::AudioStreamPrivate::ensureResamplerInitialized()
{
    if (!m_initialized)
    {
        m_initCode = initializeResampler();
        checkIoError(m_initCode);
        if (m_initCode < 0)
        {
            setLastError(m_initCode);
            if (m_ioError)
                terminate();
        }
    }
    return m_initialized;
}

int AudioStream1::AudioStreamPrivate::initializeInputFormat()
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

    m_inputFormat = std::move(inputFormat);

    return 0;
}

int AudioStream1::AudioStreamPrivate::initializeDecoder()
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

int AudioStream1::AudioStreamPrivate::initializeEncoder()
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

int AudioStream1::AudioStreamPrivate::initializeResampledFrame()
{
    auto resampledFrame = std::make_unique<ffmpeg::Frame>();
    auto context = m_encoder->codecContext();

    static constexpr int kDefaultFrameSize = 2000;

    int nbSamples = context->frame_size ? context->frame_size : kDefaultFrameSize;

    int result = resampledFrame->getBuffer(
        context->sample_fmt,
        nbSamples,
        context->channel_layout);

    if (result < 0)
        return result;

    resampledFrame->frame()->sample_rate = context->sample_rate;

    m_resampledFrame = std::move(resampledFrame);

    return result;
}

int AudioStream1::AudioStreamPrivate::initalizeResampleContext(const ffmpeg::Frame * frame)
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

int AudioStream1::AudioStreamPrivate::decodeNextFrame(ffmpeg::Frame * outFrame)
{
    for(;;)
    {
        auto packet = popNextPacket();
        if (!packet)
        {
            thisThreadSleep();
            continue;
        }

        int result = m_decoder->sendPacket(packet->packet());
        if (result < 0 && result != AVERROR(EAGAIN))
            return result;

        result = m_decoder->receiveFrame(outFrame->frame());
        if (result == 0)
            return result;
        if (result < 0 && result != AVERROR(EAGAIN))
            return result;
    }
}

int AudioStream1::AudioStreamPrivate::resample(
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

std::chrono::milliseconds AudioStream1::AudioStreamPrivate::resampleDelay() const
{
    return m_resampleContext 
        ? std::chrono::milliseconds(swr_get_delay(m_resampleContext, kMsecInSec))
        : std::chrono::milliseconds(0);
}

int AudioStream1::AudioStreamPrivate::encode(const ffmpeg::Frame* frame, ffmpeg::Packet * outPacket)
{
    int result = m_encoder->sendFrame(frame->frame());
    if (result < 0 && result != AVERROR(EAGAIN))
        return result;

    return m_encoder->receivePacket(outPacket->packet());
}

int AudioStream1::AudioStreamPrivate::resampleAudio(ffmpeg::Packet * output)
{
    int result = 0;
    for(;;)
    {
        // need to drain the the resampler periodically to avoid increasing audio delay
        if(m_resampleContext && resampleDelay() > timePerVideoFrame())
        {
            result = resample(nullptr, m_resampledFrame.get());
            if (result < 0)
                continue;
        }
        else
        {
            result = decodeNextFrame(m_decodedFrame.get());
            if (result < 0)
                return result;

            result = resample(m_decodedFrame.get(), m_resampledFrame.get());
            if(result < 0)
                return result;
        }

        result = encode(m_resampledFrame.get(), output);
        if (result == 0)
            break;
    }

    // Get duration before injecting into the packet because injection erases other fields.
    int64_t duration = output->packet()->duration;

    // Inject ADTS header into each packet becuase AAC encoder does not do it.
    result = m_adtsInjector.inject(output);
    if (result < 0)
        return result;

    output->setTimestamp(calculateTimestamp(duration));

    return result;
}

uint64_t AudioStream1::AudioStreamPrivate::calculateTimestamp(int64_t duration)
{
    uint64_t now = m_timeProvider->millisSinceEpoch();

    const AVRational sourceRate = { 1, m_encoder->sampleRate() };
    static const AVRational kTargetRate = { 1, 1000 };
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

std::chrono::milliseconds AudioStream1::AudioStreamPrivate::timePerVideoFrame() const
{
    if(auto cam = m_camera.lock())
        return cam->videoStream()->actualTimePerFrame();
    return std::chrono::milliseconds(0); //< Should never happen
}

bool AudioStream1::AudioStreamPrivate::checkIoError(int ffmpegError)
{
    m_ioError = ffmpegError == AVERROR(EIO) //< Windows and Linux.
        || ffmpegError == AVERROR(ENODEV) //< Catch all.
        || ffmpegError == AVERROR(EBUSY); //< Device is busy during initialization.
    return m_ioError;
}

void AudioStream1::AudioStreamPrivate::setLastError(int ffmpegError)
{
    if (auto cam = m_camera.lock())
        cam->setLastError(ffmpegError);
}

void AudioStream1::AudioStreamPrivate::terminate()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_terminated = true;
    m_wait.notify_all();
}

void AudioStream1::AudioStreamPrivate::tryToStartIfNotStarted()
{
    std::lock_guard<std::mutex> lock(m_threadStartMutex);
    if (m_terminated)
    {
        if (pluggedIn())
            start();
    }
}

void AudioStream1::AudioStreamPrivate::start()
{
    if (m_audioThread.joinable())
        m_audioThread.join();

    if (m_resampleThread.joinable())
        m_resampleThread.join();

    m_terminated = false;
    m_resampleThread = std::thread(&AudioStream1::AudioStreamPrivate::runResampleThread, this);
    m_audioThread = std::thread(&AudioStream1::AudioStreamPrivate::runAudioCaptureThread, this);
}

void AudioStream1::AudioStreamPrivate::stop()
{
    terminate();

    if (m_resampleThread.joinable())
        m_resampleThread.join();

    if (m_audioThread.joinable())
        m_audioThread.join();

    uninitialize();
}

void AudioStream1::AudioStreamPrivate::runAudioCaptureThread()
{
    while (!m_terminated)
    {
        auto t = m_timeProvider->millisSinceEpoch();

        if (noConsumers())
        {
            m_inputFormat.reset(nullptr);
            thisThreadSleep();
            continue;
        }

        if (!ensureFormatInitialized())
            continue;

        auto packet = 
            std::make_shared<ffmpeg::Packet>(m_inputFormat->audioCodecId(), AVMEDIA_TYPE_AUDIO);

        int result = readFrame(packet.get());

        checkIoError(result); // < Always check if it was an io error to reset m_ioError to false.

        if (result < 0)
        {
            setLastError(result);
            if (m_ioError)
                terminate();
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(m_resampleBufferMutex);
            m_resampleBuffer.push_back(packet);
        }

        auto tt = m_timeProvider->millisSinceEpoch();
        //std::cout << "capture: " << tt - t << std::endl;
    }
}

void AudioStream1::AudioStreamPrivate::runResampleThread()
{
    while (!m_terminated)
    {
        auto t = m_timeProvider->millisSinceEpoch();

        if (noConsumers())
        {
            thisThreadSleep();
            continue;
        }

        if (!m_inputFormat || !ensureResamplerInitialized())
        {
            thisThreadSleep();
            continue;
        }

        auto encodedPacket = 
            std::make_shared<ffmpeg::Packet>(m_encoder->codecId(), AVMEDIA_TYPE_AUDIO);

        if (resampleAudio(encodedPacket.get()) < 0)
        {
            thisThreadSleep();
            continue;
        }
        
        {
            std::lock_guard<std::mutex>lock(m_mutex);
            m_packetConsumerManager->givePacket(encodedPacket);
        }
        {
            auto tt = m_timeProvider->millisSinceEpoch();
            std::lock_guard<std::mutex>lock(m_resampleBufferMutex);
            //std::cout << "resample: " << tt - t << ", buf size: " << m_resampleBuffer.size() << std::endl;
        }
    }
}

int AudioStream1::AudioStreamPrivate::readFrame(ffmpeg::Packet * packet)
{
        int result;
        if (m_inputFormat->formatContext()->flags & AVFMT_FLAG_NONBLOCK)
        {
            static constexpr std::chrono::milliseconds kTimeout =
                std::chrono::milliseconds(3000);
            result = m_inputFormat->readFrameNonBlock(packet->packet(), kTimeout);
            if (result < AVERROR(EAGAIN)) //< Treat a timeout as an error.
                return result;
        }
        else
        {
            result = m_inputFormat->readFrame(packet->packet());
        }

        packet->setTimestamp(m_timeProvider->millisSinceEpoch());

        return result;
}

//-------------------------------------------------------------------------------------------------
// AudioStream1

AudioStream1::AudioStream1(
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

void AudioStream1::setEnabled(bool enabled)
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

bool AudioStream1::enabled() const
{
    return m_streamReader != nullptr;
}

bool AudioStream1::ioError() const
{
    return m_streamReader ? m_streamReader->ioError() : false;
}

void AudioStream1::addPacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    if (m_streamReader)
        m_streamReader->addPacketConsumer(consumer);
    else
        m_packetConsumerManager->addConsumer(consumer);
}

void AudioStream1::removePacketConsumer(const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    if (m_streamReader)
        m_streamReader->removePacketConsumer(consumer);
    else
        m_packetConsumerManager->removeConsumer(consumer);
}

} //namespace usb_cam
} //namespace nx 

#endif // defined(AUDIO_STREAM_1)