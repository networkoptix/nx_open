#include "audio_stream_reader.h"

extern "C" {
#include <libswresample/swresample.h>
} // extern "C"

#include <nx/utils/log/log.h>
#include <plugins/plugin_container_api.h>

#include "default_audio_encoder.h"
#include "ffmpeg/utils.h"

namespace nx {
namespace usb_cam {

namespace {

int constexpr kRetryLimit = 10;

const char * ffmpegDeviceType()
{
#ifdef _WIN32
    return "dshow";
#elif __linux__
    return "alsa";
#endif
}

} // namespace

//////////////////////////////////// AudioStreamReaderPrivate //////////////////////////////////////

AudioStreamReader::AudioStreamReaderPrivate::AudioStreamReaderPrivate(
    const std::string& url,
    nxpl::TimeProvider * timeProvider,
    const std::shared_ptr<PacketConsumerManager>& packetConsumerManager) 
    :
    m_url(url),
    m_timeProvider(timeProvider),
    m_packetConsumerManager(packetConsumerManager),
    m_initialized(false),
    m_initCode(0),
    m_retries(0),
    m_terminated(false),
    m_resampleContext(nullptr),
    m_packetCount(std::make_shared<std::atomic_int>())
{
    m_timeProvider->addRef();
    std::cout << "AudioStreamReaderPrivate" << std::endl;
    start();
}

AudioStreamReader::AudioStreamReaderPrivate::~AudioStreamReaderPrivate()
{
    m_timeProvider->releaseRef();
    std::cout << "~AudioStreamReaderPrivate" << std::endl;
    stop();
    uninitialize();
}

void AudioStreamReader::AudioStreamReaderPrivate::addPacketConsumer(const std::weak_ptr<PacketConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_packetConsumerManager->addConsumer(consumer, false);
    m_wait.notify_all();
}

void AudioStreamReader::AudioStreamReaderPrivate::removePacketConsumer(const std::weak_ptr<PacketConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_packetConsumerManager->removeConsumer(consumer);
    m_wait.notify_all();
}

int AudioStreamReader::AudioStreamReaderPrivate::sampleRate() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_encoder ? m_encoder->codecContext()->sample_rate : 0;
}

int AudioStreamReader::sampleRate() const
{
    return m_streamReader ? m_streamReader->sampleRate() : 0;
}

std::string AudioStreamReader::AudioStreamReaderPrivate::ffmpegUrl() const
{
#ifdef _WIN32
    return std::string("audio=") + m_url;
#else
    return m_url;
#endif
}

void AudioStreamReader::AudioStreamReaderPrivate::waitForConsumers()
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

int AudioStreamReader::AudioStreamReaderPrivate::initialize()
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

    m_initialized = true;
    return 0;
}

void AudioStreamReader::AudioStreamReaderPrivate::uninitialize()
{
    m_packetConsumerManager->consumerFlush();

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

bool AudioStreamReader::AudioStreamReaderPrivate::ensureInitialized()
{
    if(!m_initialized)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_initCode = initialize();
        // if(m_initCode < 0) todo inject the camera into this
        //     m_camera->setLastError(m_initCode);
    }
    return m_initialized;
}

int AudioStreamReader::AudioStreamReaderPrivate::initializeInputFormat()
{
    auto inputFormat = std::make_unique<ffmpeg::InputFormat>();
    int result = inputFormat->initialize(ffmpegDeviceType());
    if (result < 0)
        return result;
    
    result = inputFormat->open(ffmpegUrl().c_str());
    if (result < 0)
        return result;

    m_inputFormat = std::move(inputFormat);
    return 0;
}

int AudioStreamReader::AudioStreamReaderPrivate::initializeDecoder()
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

int AudioStreamReader::AudioStreamReaderPrivate::initializeEncoder()
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

int AudioStreamReader::AudioStreamReaderPrivate::initializeResampledFrame()
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

int AudioStreamReader::AudioStreamReaderPrivate::initalizeResampleContext(const AVFrame * frame)
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

int AudioStreamReader::AudioStreamReaderPrivate::decodeNextFrame(AVFrame * outFrame)
{
    int result = 0;
    while (true)
    {
        ffmpeg::Packet packet(m_inputFormat->audioCodecID());
        result = m_inputFormat->readFrame(packet.packet());
        if (result < 0)
            return result;

        m_timeStamps.addTimeStamp(packet.pts(), m_timeProvider->millisSinceEpoch());

        result = m_decoder->sendPacket(packet.packet());
        if(result < 0 && result != AVERROR(EAGAIN))
            return result;

        result = m_decoder->receiveFrame(outFrame);
        if (result == 0)
            return result;
        else if(result < 0 && result != AVERROR(EAGAIN))
            return result;
    }

    return result;
}

int AudioStreamReader::AudioStreamReaderPrivate::resample(const AVFrame * frame, AVFrame * outFrame)
{
    if(!m_resampleContext)
    {
        m_initCode = initalizeResampleContext(frame);
        if(m_initCode < 0)
            return m_initCode;
    }

    if(!m_resampleContext)
        return m_initCode;

    int result = swr_convert_frame(m_resampleContext, outFrame, frame);

    return result;
}

std::shared_ptr<ffmpeg::Packet> AudioStreamReader::AudioStreamReaderPrivate::getNextData(int * outError)
{
    #define returnData(res, retObj) do{ *outError = res; return retObj; }while(0)

    int result = 0;
    auto packet = std::make_shared<ffmpeg::Packet>(m_encoder->codecID(), m_packetCount);
    while(true)
    {
        ffmpeg::Frame frame;
        result = decodeNextFrame(frame.frame());
        if (result < 0)
            returnData(result, nullptr);

        result = resample(frame.frame(), m_resampledFrame->frame());
        if(result < 0)
        {
            ++m_retries;
            returnData(result, nullptr);
        }

        result = m_encoder->sendFrame(m_resampledFrame->frame());
        if (result < 0 && result != AVERROR(EAGAIN))
            returnData(result, nullptr);

        result = m_encoder->receivePacket(packet->packet());
        if (result == 0)
        {
            /**
             * pcm_s16le encoder doesn't set pts on packets, 
             * so if we're using it we have to set it manually
             */
            if (packet->pts() == AV_NOPTS_VALUE)
                packet->packet()->pts = frame.pts();
            break;
        }
        else if (result < 0 && result != AVERROR(EAGAIN))
            returnData(result, nullptr);
    }

    int64_t nxTimeStamp;
    if (!m_timeStamps.getNxTimeStamp(packet->pts(), &nxTimeStamp, true/*eraseEntry*/))
        nxTimeStamp = m_timeProvider->millisSinceEpoch();

    packet->setTimeStamp(nxTimeStamp);

    returnData(result, packet);
}

void AudioStreamReader::AudioStreamReaderPrivate::start()
{
    m_terminated = false;
    m_runThread = std::thread(&AudioStreamReader::AudioStreamReaderPrivate::run, this);
}

void AudioStreamReader::AudioStreamReaderPrivate::stop()
{
    m_terminated = true;
    m_wait.notify_all();
    if (m_runThread.joinable())
        m_runThread.join();
}

void AudioStreamReader::AudioStreamReaderPrivate::run()
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
            continue;

        packet->setTimeStamp(m_timeProvider->millisSinceEpoch());

        std::lock_guard<std::mutex> lock(m_mutex);
        m_packetConsumerManager->givePacket(packet);
    }
}


//////////////////////////////////////// AudioStreamReader /////////////////////////////////////////


AudioStreamReader::AudioStreamReader(
    const std::string url,
    nxpl::TimeProvider* timeProvider,
    bool enabled) 
    :
    m_url(url),
    m_timeProvider(timeProvider),
    m_packetConsumerManager(new PacketConsumerManager())
{
    m_timeProvider->addRef();
    setEnabled(enabled);
}

AudioStreamReader::~AudioStreamReader()
{
    m_timeProvider->releaseRef();
}

std::string AudioStreamReader::url() const
{
    return m_url;
}

void AudioStreamReader::setEnabled(bool enabled)
{
    if (enabled)
    {
        if(!m_streamReader)
        {
            m_streamReader = std::make_unique<AudioStreamReaderPrivate>(
                m_url,
                m_timeProvider,
                m_packetConsumerManager);
        }
    }
    else
        m_streamReader.reset(nullptr);
}

bool AudioStreamReader::enabled() const
{
    return m_streamReader != nullptr;
}

void AudioStreamReader::addPacketConsumer(const std::weak_ptr<PacketConsumer>& consumer)
{
    if (m_streamReader)
        m_streamReader->addPacketConsumer(consumer);
    else
        m_packetConsumerManager->addConsumer(consumer);
}

void AudioStreamReader::removePacketConsumer(const std::weak_ptr<PacketConsumer>& consumer)
{
    if (m_streamReader)
        m_streamReader->removePacketConsumer(consumer);
    else
        m_packetConsumerManager->removeConsumer(consumer);
}

} //namespace usb_cam
} //namespace nx 