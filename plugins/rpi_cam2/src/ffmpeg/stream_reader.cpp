#include "stream_reader.h"

#include <algorithm>

#include <nx/utils/log/log.h>
#include <plugins/plugin_container_api.h>

#include "../utils/time_profiler.h"
#include "../utils/utils.h"
#include "utils.h"
#include "input_format.h"
#include "codec.h"
#include "packet.h"
#include "frame.h"
#include "error.h"

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

} // namespace

StreamReader::StreamReader(
    const char * url,
    const CodecParameters& codecParameters,
    nxpl::TimeProvider *const timeProvider)
    :
    m_url(url),
    m_codecParams(codecParameters),
    m_timeProvider(timeProvider)
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
    m_consumers.push_back(consumer);
    updateUnlocked();
}

void StreamReader::removeConsumer(const std::weak_ptr<StreamConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    int index = 0;
    for(const auto& c : m_consumers)
    {
        if(c.lock() == consumer.lock())
            break;
        ++index;
    }

    if(index < m_consumers.size())
        m_consumers.erase(m_consumers.begin() + index);

    updateUnlocked();
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
        debug("ffmpeg::StreamReader selected fps: %d\n", largest);
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
            if (width > largestWidth || height > largestHeight)
            {
                largestWidth = width;
                largestHeight = height;
            }
        }
    }

    if(m_codecParams.width != largestWidth || m_codecParams.height != largestHeight)
    {
        debug("ffmpeg::StreamReader selected Resolution: %d, %d\n", largestWidth, largestHeight);
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
        debug("ffmpeg::StreamReader selected bitrate: %d\n", largest);
        m_codecParams.bitrate = largest;
        m_cameraState = kModified;
    }
}

void StreamReader::updateUnlocked()
{
    updateFpsUnlocked();
    updateResolutionUnlocked();
    updateBitrateUnlocked();
}

void StreamReader::start()
{
    if(m_started)
        return;

    m_started = true;
    m_terminated = false;
    m_runThread = std::thread(&StreamReader::run, this);
}

void StreamReader::stop()
{
    if(m_terminated)
        return;

    m_terminated = true;
    m_started = false;
    if(m_runThread.joinable())
        m_runThread.join();
}

StreamReader::CameraState StreamReader::cameraState() const
{
    return m_cameraState;
}

int StreamReader::gopSize() const
{
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

void StreamReader::run()
{
    while (!m_terminated)
    {
        if (m_consumers.empty())
            continue;

        if(!ensureInitialized())
            continue;

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
    auto inputFormat = std::make_unique<InputFormat>();

    int initCode = inputFormat->initialize(deviceType());
    if (initCode < 0)
        return initCode;

    setInputFormatOptions(inputFormat);

    int openCode = inputFormat->open(m_url.c_str());
    if (openCode < 0)
        return openCode;

    m_inputFormat = std::move(inputFormat);

    debug("");
    av_dump_format(m_inputFormat->formatContext(), 0, m_url.c_str(), 0);
    m_cameraState = kInitialized;
    
    debug("initialize done\n");

    return 0;
}

void StreamReader::uninitialize()
{
    m_inputFormat.reset(nullptr);
    m_cameraState = kOff;
}

void StreamReader::setInputFormatOptions(const std::unique_ptr<InputFormat>& inputFormat)
{
    AVFormatContext * context = inputFormat->formatContext();
    context->video_codec_id = m_codecParams.codecID;
    context->flags |= AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_DISCARD_CORRUPT;
    
    inputFormat->setFps(m_codecParams.fps);
    inputFormat->setResolution(m_codecParams.width, m_codecParams.height);
}

} // namespace ffmpeg
} // namespace nx