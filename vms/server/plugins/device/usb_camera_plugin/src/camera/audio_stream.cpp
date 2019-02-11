#include "audio_stream.h"

#include <algorithm>

#include <plugins/plugin_container_api.h>
#include <nx/utils/log/log.h>

#include "camera.h"
#include "device/audio/utils.h"

namespace nx::usb_cam {

namespace {

static const char * ffmpegDeviceTypePlatformDependent()
{
#ifdef _WIN32
    return "dshow";
#elif __linux__
    return "alsa";
#endif
}

static constexpr int kResyncThresholdMsec = 1000;

} // namespace

AudioStream::AudioStream(const std::string& url, const std::weak_ptr<Camera>& camera, bool enabled):
    m_url(url),
    m_camera(camera),
    m_timeProvider(camera.lock()->timeProvider())
{
    setEnabled(enabled);
}

AudioStream::~AudioStream()
{
    stop();
    m_timeProvider->releaseRef();
}

bool AudioStream::ioError() const
{
    return m_ioError;
}

void AudioStream::setEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (enabled)
    {
        if (!m_packetConsumerManager.empty())
            start();
    }
    else
    {
        stop();
    }
    m_enabled = enabled;
}

bool AudioStream::enabled() const
{
    return m_enabled;
}

void AudioStream::addPacketConsumer(
    const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_packetConsumerManager.addConsumer(consumer);
    if (m_enabled)
        start();
}

void AudioStream::removePacketConsumer(
    const std::weak_ptr<AbstractPacketConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_packetConsumerManager.removeConsumer(consumer);
    if (m_packetConsumerManager.empty())
        stop();
}

std::string AudioStream::ffmpegUrlPlatformDependent() const
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

int AudioStream::initialize()
{
    int status = initializeInputFormat();
    if (status < 0)
        return status;

    AVStream * stream = m_inputFormat->findStream(AVMEDIA_TYPE_AUDIO);
    if (!stream)
        return AVERROR_DECODER_NOT_FOUND;

    status = m_transcoder.initialize(stream);
    if (status < 0)
        return status;
    m_initialized = true;
    return 0;
}

void AudioStream::uninitialize()
{
    m_transcoder.uninitialize();
    m_inputFormat.reset(nullptr);
    m_initialized = false;
}

bool AudioStream::ensureInitialized()
{
    if (!m_initialized)
    {
        int status = initialize();
        checkIoError(status);
        if (status < 0)
        {
            setLastError(status);
            if (m_ioError)
                m_terminated = true;
        }
    }
    return m_initialized;
}

int AudioStream::initializeInputFormat()
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

void AudioStream::setLastError(int ffmpegError)
{
    if (auto cam = m_camera.lock())
        cam->setLastError(ffmpegError);
}

void AudioStream::start()
{
    if (!device::audio::pluggedIn(m_url))
    {
        NX_WARNING(this, "Audio device not plugged in");
        return;
    }

    stop();
    m_terminated = false;
    m_audioThread = std::thread(&AudioStream::run, this);
}

void AudioStream::stop()
{
    m_terminated = true;
    if (m_audioThread.joinable())
        m_audioThread.join();
}

int AudioStream::nextPacket(std::shared_ptr<ffmpeg::Packet>& resultPacket)
{
    int status = m_transcoder.receivePacket(resultPacket);
    if (status == 0 || (status < 0 && status != AVERROR(EAGAIN)))
        return status;

    ffmpeg::Packet packet(m_inputFormat->audioCodecId(), AVMEDIA_TYPE_AUDIO);
    // AVFMT_FLAG_NONBLOCK is set if running on Windows. see
    if (m_inputFormat->formatContext()->flags & AVFMT_FLAG_NONBLOCK)
    {
        static constexpr std::chrono::milliseconds kTimeout =
            std::chrono::milliseconds(1000);
        status = m_inputFormat->readFrameNonBlock(packet.packet(), kTimeout);
        if (status < AVERROR(EAGAIN)) //< Treaat a timeout as an error.
            status = AVERROR(EIO);
    }
    else
    {
        status = m_inputFormat->readFrame(packet.packet());
    }
    if (status < 0)
        return status;
    status = m_transcoder.sendPacket(packet);
    if (status < 0)
        return status;

    return status;
}

uint64_t AudioStream::calculateTimestamp(int64_t duration)
{
    uint64_t now = m_timeProvider->millisSinceEpoch();

    const AVRational sourceRate = { 1, m_transcoder.targetSampleRate() };
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

bool AudioStream::checkIoError(int ffmpegError)
{
    m_ioError = ffmpegError == AVERROR(EIO) //< Windows and Linux.
        || ffmpegError == AVERROR(ENODEV) //< Catch all.
        || ffmpegError == AVERROR(EBUSY); //< Device is busy during initialization.
    return m_ioError;
}

void AudioStream::run()
{
    while (!m_terminated)
    {
        if (!ensureInitialized())
            continue;

        std::shared_ptr<ffmpeg::Packet> packet;
        int status = nextPacket(packet);
        checkIoError(status); // < Always check if it was an io error to reset m_ioError to false.
        if (status < 0)
        {
            setLastError(status);
            if (m_ioError)
                m_terminated = true;

            continue;
        }

        // If the encoder is AAC, some packets are buffered and copied before delivering.
        // In that case, packet is nullptr.
        if (packet)
        {
            packet->setTimestamp(calculateTimestamp(packet->packet()->duration));
            m_packetConsumerManager.pushPacket(packet);
        }
    }
    uninitialize();
}

} // namespace nx::usb_cam
