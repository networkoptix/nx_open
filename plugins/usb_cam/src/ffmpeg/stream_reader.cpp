#include "stream_reader.h"

#include <algorithm>
#ifdef _WIN32
#elif __linux__
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#endif

#include <nx/utils/log/log.h>
#include <plugins/plugin_container_api.h>

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
#ifdef _WIN32
#elif __linux__
    int fileDescriptor = open(devicePath, O_RDWR);
    struct v4l2_ext_controls ecs;
    struct v4l2_ext_control ec;
    memset(&ecs, 0, sizeof(ecs));
    memset(&ec, 0, sizeof(ec));
    ec.id = V4L2_CID_MPEG_VIDEO_BITRATE;
    ec.value = bitrate;
    ec.size = 0;
    ecs.controls = &ec;
    ecs.count = 1;
    ecs.ctrl_class = V4L2_CTRL_CLASS_MPEG;
    ioctl(fileDescriptor, VIDIOC_S_EXT_CTRLS, &ecs);
    if(fileDescriptor != -1)
        close(fileDescriptor);
#endif
}

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
    m_lastFfmpegError(0)
{
    start();
}

StreamReader::~StreamReader()
{
    stop();
    uninitialize();
}

void StreamReader::addConsumer(const std::weak_ptr<StreamConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (consumerIndex(consumer) >= m_consumers.size())
    {
        m_consumers.push_back(consumer);
        updateUnlocked();
        if(m_consumers.size() == 1)
            m_wait.notify_one();
    }
}

void StreamReader::removeConsumer(const std::weak_ptr<StreamConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    int index = consumerIndex(consumer);
    if(index < m_consumers.size())
        m_consumers.erase(m_consumers.begin() + index);

    updateUnlocked();
}

int StreamReader::initializeDecoderFromStream(Codec *decoder) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_inputFormat)
        return AVERROR_DECODER_NOT_FOUND;

    AVStream * stream = m_inputFormat->findStream(AVMEDIA_TYPE_VIDEO);
    if (!stream)
        return AVERROR_DECODER_NOT_FOUND;
    
    return decoder->initializeDecoder(stream->codecpar);
}

std::string StreamReader::ffmpegUrl () const
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
    if(m_consumers.empty())
        return;

    int largest = 0;
    for (const auto & consumer : m_consumers)
    {
        if (auto c = consumer.lock())
        {
            int fps = c->fps();
            if(fps > largest)
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
    if (m_consumers.empty())
        return;

    int largestWidth = 0;
    int largestHeight = 0;
    for (const auto & consumer : m_consumers)
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
    if (m_consumers.empty())
        return;

    int largest = 0;
    for (const auto & consumer : m_consumers)
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

int StreamReader::consumerIndex (const std::weak_ptr<StreamConsumer> &consumer)
{
    int index = 0;
    for (const auto& c : m_consumers)
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
    if(m_runThread.joinable())
        m_runThread.join();
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

std::string StreamReader::url () const
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
            continue;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_consumers.empty())
                m_wait.wait(lock);
        }

        auto packet = std::make_shared<Packet>(m_inputFormat->videoCodecID());
        int readCode = m_inputFormat->readFrame(packet->packet());
        if (readCode < 0)
            continue;

        packet->setTimeStamp(m_timeProvider->millisSinceEpoch());

        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto & consumer : m_consumers)
        {
            if (auto c = consumer.lock())
                c->givePacket(packet);
        }
    }
}

bool StreamReader::ensureInitialized()
{
    switch(m_cameraState)
    {
        case kModified:
            NX_DEBUG(this) << "ensureInitialized(): codec parameters modified, reinitializing";
            uninitialize();
            initialize();
            break;
        case kOff:
            NX_DEBUG(this) << "ensureInitialized(): first initialization";
            initialize();
            break;
    }

    return m_cameraState == kInitialized;
}

int StreamReader::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
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
    m_cameraState = kInitialized;

    return 0;
}

void StreamReader::uninitialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_inputFormat.reset(nullptr);
    m_cameraState = kOff;
}

void StreamReader::setInputFormatOptions(const std::unique_ptr<InputFormat>& inputFormat)
{
    AVFormatContext * context = inputFormat->formatContext();
    if(m_codecParams.codecID != AV_CODEC_ID_NONE)
        context->video_codec_id = m_codecParams.codecID;
    
    if(m_codecParams.fps != 0)
        inputFormat->setFps(m_codecParams.fps);

    if(m_codecParams.width * m_codecParams.height > 0)
        inputFormat->setResolution(m_codecParams.width, m_codecParams.height);

    
    if(m_codecParams.bitrate > 0)
    {
        /*ffmpeg doesn't have an option for setting the bitrate.*/
        setBitrate(m_url.c_str(), m_codecParams.bitrate, utils::toNxCompressionType(m_codecParams.codecID));
    }
}

} // namespace ffmpeg
} // namespace nx