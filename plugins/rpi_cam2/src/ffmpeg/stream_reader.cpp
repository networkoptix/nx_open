#include "stream_reader.h"

#include <algorithm>

#include <nx/utils/log/log.h>

#include "../utils/utils.h"
#include "utils.h"
#include "input_format.h"
#include "codec.h"
#include "packet.h"
#include "frame.h"
#include "error.h"
#include "../utils/time_profiler.h"

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

}

StreamReader::StreamReader(const char * url, const CodecParameters& codecParameters):
    m_url(url),
    m_codecParams(codecParameters)
{
}

StreamReader::~StreamReader()
{
    uninitialize();
}

int StreamReader::addRef()
{
    return ++m_refCount;
}

int StreamReader::releaseRef()
{
    m_refCount = m_refCount > 0 ? m_refCount - 1 : 0;
    return m_refCount;
}

void StreamReader::start()
{
    if(m_started)
        return;

    m_started = true;
    std::thread t(&StreamReader::run, this);
    t.detach();
}

void StreamReader::run()
{
    while (!m_terminated)
    {
        //wait();
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_consumers.empty())
            continue;

        auto packet = std::make_shared<Packet>();
        int readCode = loadNextData(packet.get());
        if (readCode < 0)
            continue;

        for (auto & consumer : m_consumers)
        {
            if (auto c = consumer.lock())
                c->givePacket(packet);
        }
    }

    m_started = false; 
    m_terminated = false;
}

void StreamReader::addConsumer(const std::weak_ptr<StreamConsumer>& consumer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_consumers.push_back(consumer);
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

    if(m_consumers.empty())
        m_terminated = true;
}

StreamReader::CameraState StreamReader::cameraState() const
{
    return m_cameraState;
}

const std::unique_ptr<ffmpeg::Codec>& StreamReader::decoder()
{
    return m_decoder;
}

const std::unique_ptr<ffmpeg::InputFormat>& StreamReader::inputFormat()
{
    return m_inputFormat;
}

void StreamReader::setFps(int fps)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if(m_consumers.empty())
    {
        m_codecParams.fps = fps;
        return;
    }

    int largest = 0;
    for (const auto & consumer : m_consumers)
    {
        if (auto c = consumer.lock())
        {
            int consumerFps = c->fps();
            if(consumerFps > largest)
                largest = consumerFps;
        }
    }
    
    if (m_codecParams.fps != largest)
    {
        debug("ffmpeg::StreamReader selected fps: %d\n", largest);
        m_codecParams.fps = largest;
        m_cameraState = kModified;
    }
}

void StreamReader::setBitrate(int bitrate)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_consumers.empty())
    {
        m_codecParams.bitrate = bitrate;
        return;
    }

    int largest = 0;
    for (const auto & consumer : m_consumers)
    {
        if (auto c = consumer.lock())
        {
            int b = c->bitrate();
            if (b > largest)
                largest = b;
        }
    }

    if (largest != m_codecParams.bitrate)
    {
        debug("Selected Bitrate: %d\n", largest);
        m_codecParams.bitrate = largest;
        m_cameraState = kModified;
    }
}

void StreamReader::setResolution(int width, int height)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_consumers.empty())
    {
        m_codecParams.setResolution(width, height);
        return;
    }

    int largestWidth = 0;
    int largestHeight = 0;
    for (const auto & consumer : m_consumers)
    {
        if (auto c = consumer.lock())
        {
            int w;
            int h;
            c->resolution(&w, &h);
            if (w > largestWidth || h > largestHeight)
            {
                largestWidth = w;
                largestHeight = h;
            }
        }
    }

    if(m_codecParams.width != largestWidth || m_codecParams.height != largestHeight)
    {
        debug("selected Resolution: %d, %d\n", largestWidth, largestHeight);
        m_codecParams.setResolution(largestWidth, largestHeight);
        m_cameraState = kModified;
    }
}

int StreamReader::loadNextData(ffmpeg::Packet * outPacket)
{
    if(!ensureInitialized())
        return error::lastError();

    outPacket->setCodecID(m_decoder->codecID());
    return m_inputFormat->readFrame(outPacket->packet());
}

const std::unique_ptr<ffmpeg::Packet>& StreamReader::currentPacket() const
{
    return m_currentPacket;
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
    auto decoder = std::make_unique<Codec>();

    int initCode = inputFormat->initialize(deviceType());
    if (initCode < 0)
        return initCode;

    setInputFormatOptions(inputFormat);

    int openCode = inputFormat->open(m_url.c_str());
    if (openCode < 0)
        return openCode;
        
    // todo initialize this differently for windows
    initCode = decoder->initializeDecoder("h264_mmal");
    if (initCode < 0)
        return initCode;

    openCode = decoder->open();
    if (openCode < 0)
        return openCode;

    debug("Selected decoder: %s\n", decoder->codec()->name);

    m_inputFormat = std::move(inputFormat);
    m_decoder = std::move(decoder);

    debug("");
    av_dump_format(m_inputFormat->formatContext(), 0, m_url.c_str(), 0);
    m_cameraState = kInitialized;
    
    debug("initialize done\n");

    return 0;
}

void StreamReader::uninitialize()
{
    if (m_decoder)
        m_decoder.reset(nullptr);

    if (m_inputFormat)
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

int StreamReader::decode(AVFrame * outFrame, const AVPacket * packet)
{
    int decodeSize = 0;
    int gotFrame = 0;
    while(!gotFrame)
    {
        decodeSize = m_decoder->decode(outFrame, &gotFrame, packet);
        if(decodeSize < 0 || gotFrame)
            break;
    }
    return decodeSize;
}

void StreamReader::wait()
{
    float sleep = 1.0 / (float) m_codecParams.fps;
    std::this_thread::sleep_for(std::chrono::milliseconds((int)(sleep * 1000)));
}

} // namespace ffmpeg
} // namespace nx
