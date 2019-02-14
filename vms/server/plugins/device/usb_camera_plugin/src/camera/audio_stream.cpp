#include "audio_stream.h"

#include <algorithm>

#include <plugins/plugin_container_api.h>
#include <nx/utils/log/log.h>

#include "camera.h"
#include "device/audio/utils.h"

namespace nx::usb_cam {

namespace {

static const char* ffmpegDeviceTypePlatformDependent()
{
#ifdef _WIN32
    return "dshow";
#elif __linux__
    return "alsa";
#endif
}

static constexpr int kResyncThresholdMsec = 1000;

} // namespace

AudioStream::AudioStream(const std::string& url, nxpl::TimeProvider* timeProvider):
    m_url(url),
    m_timeProvider(timeProvider)
{
}

AudioStream::~AudioStream()
{
    uninitialize();
}

void AudioStream::updateUrl(const std::string& url)
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    m_url = url;
    m_transcoder.uninitialize();
    m_inputFormat.reset(nullptr);
    m_initialized = false;
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
    int status = initializeInput();
    if (status < 0)
        return status;

    AVStream * stream = m_inputFormat->findStream(AVMEDIA_TYPE_AUDIO);
    if (!stream)
        return AVERROR_DECODER_NOT_FOUND;

    return m_transcoder.initialize(stream);
}

void AudioStream::uninitialize()
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    m_transcoder.uninitialize();
    m_inputFormat.reset(nullptr);
    m_initialized = false;
}

int AudioStream::initializeInput()
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

#endif // _WIN32

    //inputFormat->formatContext()->flags |= AVFMT_FLAG_NONBLOCK;
    result = inputFormat->open(ffmpegUrlPlatformDependent().c_str());
    if (result < 0)
        return result;

    m_inputFormat = std::move(inputFormat);

    return 0;
}

int AudioStream::nextPacket(std::shared_ptr<ffmpeg::Packet>& resultPacket)
{
    std::scoped_lock<std::mutex> lock(m_mutex);
    int status;
    if (!m_initialized)
    {
        status = initialize();
        if (status < 0)
            return status;
        m_initialized = true;
    }
    status = m_transcoder.receivePacket(resultPacket);
    if (status == 0 || (status < 0 && status != AVERROR(EAGAIN)))
    {
        if (status == 0)
            resultPacket->setTimestamp(calculateTimestamp(resultPacket->packet()->duration));
        return status;
    }

    resultPacket.reset();
    ffmpeg::Packet packet(m_inputFormat->audioCodecId(), AVMEDIA_TYPE_AUDIO);
    status = m_inputFormat->readFrame(packet.packet());
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

} // namespace nx::usb_cam
