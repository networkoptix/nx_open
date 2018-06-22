#include "stream_reader.h"

#ifdef __linux__
#include <errno.h>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
} // extern "C"

#include <nx/utils/log/log.h>

#include "../utils/utils.h"
#include "utils.h"
#include "input_format.h"
#include "codec.h"
#include "error.h"

namespace nx {
namespace webcam_plugin {
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

StreamReader::StreamReader(const char * url, const CodecContext& codecParameters):
    m_url(url),
    m_codecParams(codecParameters)
{
    debug("%s\n", __FUNCTION__);
}

StreamReader::~StreamReader()
{
    uninitialize();
}

int StreamReader::addRef()
{
    return ++m_refCount;
}

int StreamReader::removeRef()
{
    m_refCount = m_refCount > 0 ? m_refCount - 1 : 0;
    if(m_refCount == 0 && m_cameraState == kInitialized)
        uninitialize();
    return m_refCount;
}

const std::unique_ptr<ffmpeg::Codec>& StreamReader::codec()
{
    return m_decoder;
}

const std::unique_ptr<ffmpeg::InputFormat>& StreamReader::inputFormat()
{
    return m_inputFormat;
}

void StreamReader::setFps(int fps)
{
    m_codecParams.setFps(fps);
    m_cameraState = kModified;
}

void StreamReader::setBitrate(int bitrate)
{
    m_codecParams.setBitrate(bitrate);
    m_cameraState = kModified;
}

void StreamReader::setResolution(int width, int height)
{
    m_codecParams.setResolution({width, height});
    m_cameraState = kModified;
}

AVFrame * StreamReader::currentFrame()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentFrame;
}

AVPacket * StreamReader::currentPacket()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentPacket;
}

int StreamReader::loadNextData()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!ensureInitialized())
        return error::lastError();

    av_packet_unref(m_currentPacket);
    av_frame_unref(m_currentFrame);
    av_init_packet(m_currentPacket);

    int decodeCode = decodeFrame(m_currentPacket, m_currentFrame);
    error::updateIfError(decodeCode);
    
    return decodeCode;
}

int StreamReader::ensureInitialized()
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

    int error = 0;
    if(m_cameraState != kInitialized)
    {
        error = initialize();
        NX_DEBUG(this) << "ensureInitialized(): first initialization";
        printError();
    }
    else if(m_cameraState == kModified)
    {
        NX_DEBUG(this) << "ensureInitialized(): codec parameters modified, reinitializing";
        uninitialize();
        error = initialize();
        printError();
    }

    return error;
}

int StreamReader::initialize()
{
    auto inputFormat = std::make_unique<InputFormat>();
    auto decoder = std::make_unique<Codec>();

    int initCode = inputFormat->initialize(deviceType());
    if (error::updateIfError(initCode))
        return initCode;

    setInputFormatOptions(inputFormat);

    // todo initialize this differently for windows
    initCode = decoder->initializeDecoder("h264_mmal");
    if (error::updateIfError(initCode))
        return initCode;

    int openCode = inputFormat->open(m_url.c_str());
    if (error::updateIfError(openCode))
        return openCode;
        
    openCode = decoder->open();
    if (error::updateIfError(openCode))
        return openCode;

    int allocCode = allocateCurrentPacket();
    if (error::updateIfError(allocCode))
        return allocCode;

    allocCode = allocateCurrentFrame();
    if (error::updateIfError(allocCode))
        return allocCode;

    m_inputFormat = std::move(inputFormat);
    m_decoder = std::move(decoder);

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

    if (m_currentPacket)
        av_packet_free(&m_currentPacket);

    if (m_currentFrame)
        av_frame_free(&m_currentFrame);

    m_cameraState = kOff;
}

int StreamReader::allocateCurrentPacket()
{
    m_currentPacket = av_packet_alloc();
    if(!m_currentPacket)
        return AVERROR(ENOMEM);
    return 0;
}

int StreamReader::allocateCurrentFrame()
{
    m_currentFrame = av_frame_alloc();
    if(!m_currentFrame)
        return AVERROR(ENOMEM);
    return 0;
}

void StreamReader::setInputFormatOptions(const std::unique_ptr<InputFormat>& inputFormat)
{
    AVFormatContext * context = inputFormat->formatContext();
    context->video_codec_id = utils::toAVCodecID(m_codecParams.codecID());
    context->flags |= AVFMT_FLAG_NOBUFFER;
    context->flags |= AVFMT_FLAG_DISCARD_CORRUPT;
    
    inputFormat->setFps(m_codecParams.fps());
    auto resolution = m_codecParams.resolution();
    inputFormat->setResolution(resolution.width, resolution.height);
}

int StreamReader::decodeFrame(AVPacket * packet, AVFrame * outFrame)
{
    int gotFrame;
    switch(m_decoder->codec()->type)
    {
        case AVMEDIA_TYPE_VIDEO:
            return m_decoder->decodeVideo(m_inputFormat->formatContext(), outFrame, &gotFrame, packet);
        case AVMEDIA_TYPE_AUDIO:
            return m_decoder->decodeAudio(m_inputFormat->formatContext(), outFrame, &gotFrame, packet);
        default:
        //todo add decode funcs for other types to Codec class
        return AVERROR_UNKNOWN;
    }
}

} // namespace ffmpeg
} // namespace webcam_plugin
} // namespace nx
