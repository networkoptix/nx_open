#include "stream_reader.h"

#ifdef __linux__
#include <errno.h>
#endif

extern "C" {
//#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
} // extern "C"

#include <nx/utils/log/log.h>

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

}

StreamReader::StreamReader(const char * url, const StreamReader::CodecParameters& codecParameters):
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

StreamReader::CameraState StreamReader::cameraState() const
{
    return m_cameraState;
}

AVCodecID StreamReader::decoderID() const
{
    return m_decoder->codecID();
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
    if(fps != m_codecParams.fps)
    {
        debug("ffmpeg::StreamReader::setFPS: %d\n", fps);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_codecParams.fps = fps;
        m_cameraState = kModified;
    }
}

void StreamReader::setBitrate(int bitrate)
{
    if(bitrate != m_codecParams.bitrate)
    {
        debug("ffmpeg::StreamReader::setBitrate: %d\n", bitrate);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_codecParams.bitrate = bitrate;
        m_cameraState = kModified;
    }
}

void StreamReader::setResolution(int width, int height)
{
    if(m_codecParams.width != width || m_codecParams.height != height)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        debug("ffmpeg::StreamReader::setResolution: %d, %d\n", width, height);
        m_codecParams.setResolution(width, height);
        m_cameraState = kModified;
    }
}

int StreamReader::loadNextData(ffmpeg::Packet * copyPacket)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!ensureInitialized())
        return error::lastError();

    if(copyPacket && m_refCount > 1)
        return m_currentPacket->copy(copyPacket);

    m_currentPacket->unreference();
    int readCode = m_inputFormat->readFrame(m_currentPacket->packet());
    if(readCode < 0)
        return readCode;

    if(copyPacket)
        return m_currentPacket->copy(copyPacket);

    return 0;
}

const std::unique_ptr<ffmpeg::Packet>& StreamReader::currentPacket() const
{
    return m_currentPacket;
}

bool StreamReader::ensureInitialized()
{
     auto printError =
        [this]()
        {
            if(!error::hasError())
                return;
#ifdef __linux__
            int err = errno;
            NX_DEBUG(this) << "ensureInitialized(): errno: " << err;
#endif
            NX_DEBUG(this) << "ensureInitialized(): ffmpeg::error::lastError" << error::lastError();
        };

    switch(m_cameraState)
    {
        case kModified:
            NX_DEBUG(this) << "ensureInitialized(): codec parameters modified, reinitializing";
            uninitialize();
            initialize();
            printError();
            break;
        case kOff:
            NX_DEBUG(this) << "ensureInitialized(): first initialization";
            initialize();
            printError();
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

    // todo initialize this differently for windows
    initCode = decoder->initializeDecoder("h264_mmal");
    if (initCode < 0)
        return initCode;

    int openCode = inputFormat->open(m_url.c_str());
    if (openCode < 0)
        return openCode;
        
    openCode = decoder->open();
    if (openCode < 0)
        return openCode;

    m_currentPacket = std::make_unique<ffmpeg::Packet>();
    m_inputFormat = std::move(inputFormat);
    m_decoder = std::move(decoder);

    debug("");
    av_dump_format(m_inputFormat->formatContext(), 0, m_url.c_str(), 0);
    m_cameraState = kInitialized;
    
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
    context->flags |= AVFMT_FLAG_NOBUFFER;
    context->flags |= AVFMT_FLAG_DISCARD_CORRUPT;
    
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
        //debug("decoding, !gotFrame, decodeSize: %d\n", decodeSize);
        if(decodeSize < 0 || gotFrame)
        {
            //debug("gotFrame, decodeSize: %d\n", decodeSize);
            break;
        }
    }
    return decodeSize;
}

} // namespace ffmpeg
} // namespace nx
