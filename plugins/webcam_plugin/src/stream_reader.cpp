/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#include "stream_reader.h"

#ifdef _WIN32
#include <Windows.h>
#undef max
#undef min
#else
#include <time.h>
#include <unistd.h>
#endif

extern "C" {
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}

#include <sys/timeb.h>

#include <memory>

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "av_utils.h"
#include "avcodec_container.h"
#include "ilp_empty_packet.h"
#include "plugin.h"

namespace {

static const nxcip::UsecUTCTimestamp USEC_IN_MS = 1000;
static const nxcip::UsecUTCTimestamp USEC_IN_SEC = 1000*1000;
static const nxcip::UsecUTCTimestamp NSEC_IN_USEC = 1000;
static const int MAX_FRAME_SIZE = 4*1024*1024;
}


namespace nx {
namespace webcam_plugin {

StreamReader::StreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    nxpl::TimeProvider *const timeProvider,
    const nxcip::CameraInfo& cameraInfo,
    int encoderNumber )
:
    m_refManager( parentRefManager ),
    m_info( cameraInfo ),
    m_encoderNumber( encoderNumber ),
    m_fps(0),
    m_bitrate(0),
    m_modified(false),
    m_terminated( false ),
    m_timeProvider(timeProvider),
    m_formatContext(nullptr),
    m_inputFormat(nullptr),
    m_options(nullptr),
    m_videoDecoder(nullptr),
    m_videoEncoder(nullptr),
    m_avVideoPacket(nullptr)
{
    NX_ASSERT(m_timeProvider);
    initializeAv();
}

StreamReader::~StreamReader()
{
    m_timeProvider->releaseRef();
    unInitializeAv();
}

//!Implementation of nxpl::PluginInterface::queryInterface
void* StreamReader::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_StreamReader, sizeof(nxcip::IID_StreamReader) ) == 0 )
    {
        addRef();
        return this;
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

//!Implementation of nxpl::PluginInterface::addRef
unsigned int StreamReader::addRef()
{
    return m_refManager.addRef();
}

//!Implementation of nxpl::PluginInterface::releaseRef
unsigned int StreamReader::releaseRef()
{
    return m_refManager.releaseRef();
}

static const unsigned int MAX_FRAME_DURATION_MS = 5000;

int StreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    if (!isValid())
        return nxcip::NX_IO_ERROR;

    int nxcipErrorCode;
    std::unique_ptr<ILPVideoPacket> nxVideoPacket;
        
    switch (m_videoDecoder->codecID())
    {
        case AV_CODEC_ID_H264:
        case AV_CODEC_ID_MJPEG:
        {
            int readCode = m_videoDecoder->readFrame(m_avVideoPacket);
            if (m_lastError.updateIfError(readCode))
                return nxcip::NX_TRY_AGAIN;

            nxVideoPacket = toNxVideoPacket(m_avVideoPacket, m_videoDecoder->codecID());
            nxcipErrorCode = nxcip::NX_NO_ERROR;
            break;
        }
        default:
            nxVideoPacket = transcodeVideo(&nxcipErrorCode);
            break;
    }

    *lpPacket = nxVideoPacket.release();

    return nxcipErrorCode;
}

void StreamReader::interrupt()
{
    std::shared_ptr<nx::network::http::HttpClient> client;
    {
        QnMutexLocker lk(&m_mutex);
        m_terminated = true;
        m_cond.wakeAll();
    }

    //closing connection
    if(client)
    {
        client->pleaseStop();
        client.reset();
    }
}

void StreamReader::setFps( float fps )
{
    m_fps = fps;
    m_modified = true;
}

void StreamReader::setResolution(const QSize& resolution)
{
    m_resolution = resolution;
    m_modified = true;
}

void StreamReader::setBitrate(int64_t bitrate)
{
    m_bitrate = bitrate;
    m_modified = true;
}

void StreamReader::updateCameraInfo( const nxcip::CameraInfo& info )
{
    m_info = info;
}

std::unique_ptr<ILPVideoPacket> StreamReader::toNxVideoPacket(AVPacket *packet, AVCodecID codecID)
{
    std::unique_ptr<ILPVideoPacket> nxVideoPacket(new ILPVideoPacket(
        &m_allocator,
        0,
        m_timeProvider->millisSinceEpoch()  * USEC_IN_MS,
        nxcip::MediaDataPacket::fKeyPacket,
        0 ) );
     
     nxVideoPacket->resizeBuffer(packet->size);
     if (nxVideoPacket->data())
         memcpy(nxVideoPacket->data(), packet->data, packet->size);

     nxVideoPacket->setCodecType(utils::av::toNxCompressionType(codecID));

     return nxVideoPacket;
}

std::unique_ptr<ILPVideoPacket> StreamReader::transcodeVideo(int * nxcipErrorCode)
{
    AVFrame* decodedFrame = getDecodedVideoFrame();
    if (!decodedFrame)
    {
        * nxcipErrorCode = nxcip::NX_NETWORK_ERROR;
        return nullptr;
    }

    AVFrame* convertedFrame = toYUV420(m_videoDecoder->codecContext(), decodedFrame);

    AVPacket * encodedPacket = av_packet_alloc();
    av_init_packet(encodedPacket);
    encodedPacket->data = NULL;
    encodedPacket->size = 0;

    int gotPacket;
    int encodeCode = m_videoEncoder->encodeVideo(encodedPacket, convertedFrame, &gotPacket);
    if (m_lastError.updateIfError(encodeCode))
    {
        * nxcipErrorCode = nxcip::NX_TRY_AGAIN;
        return nullptr;
    }
        
    auto nxVideoPacket = toNxVideoPacket(encodedPacket, m_videoEncoder->codecID());

    av_frame_free(&decodedFrame);
    av_frame_free(&convertedFrame);
    av_free_packet(encodedPacket);

    *nxcipErrorCode = nxcip::NX_NO_ERROR;
    return nxVideoPacket;
}



AVFrame * StreamReader::getDecodedVideoFrame()
{
    AVFrame * decodedFrame = av_frame_alloc();
    int gotPicture = 0;
    while (!gotPicture)
    {
        int returnCode = m_videoDecoder->decodeVideo(decodedFrame, &gotPicture, m_avVideoPacket);
        if (m_lastError.updateIfError(returnCode))
            return nullptr;
    }
   
    return decodedFrame;
}

AVPacket * StreamReader::getEncodedVideoPacket(AVFrame * frame)
{
    AVPacket* packet = av_packet_alloc();
    av_init_packet(packet);
    packet->data = NULL;
    packet->size = 0;

    int gotPacket;
    int encodeCode = m_videoEncoder->encodeVideo(packet, frame, &gotPacket);
    
    if (encodeCode < 0 || !gotPacket)
    {
        av_packet_free(&packet);
        return nullptr;
    }

    return packet;
}

bool StreamReader::isValid() const
{
    return m_formatContext != nullptr
        && m_inputFormat != nullptr
        && m_videoDecoder != nullptr
        && m_videoEncoder != nullptr;
}

AVFrame * StreamReader::toYUV420(AVCodecContext * codecContext, AVFrame * frame)
{
    AVFrame* resultFrame = av_frame_alloc();

    AVPixelFormat pix_fmt = utils::av::suggestPixelFormat(codecContext->codec_id);

    av_image_alloc(resultFrame->data, resultFrame->linesize, frame->width, frame->height,
        pix_fmt, 32);

   struct SwsContext * img_convert_ctx = sws_getCachedContext(
        NULL, codecContext->width, codecContext->height,
        utils::av::unDeprecatePixelFormat(codecContext->pix_fmt),
        codecContext->width, codecContext->height, pix_fmt,
        SWS_BICUBIC, NULL, NULL, NULL);

    sws_scale(img_convert_ctx, frame->data, frame->linesize, 0, codecContext->height,
        resultFrame->data, resultFrame->linesize);

    // ffmpeg prints warnings if these fields are not explicitly set
    resultFrame->width = frame->width;
    resultFrame->height = frame->height;
    resultFrame->format = pix_fmt;

    sws_freeContext(img_convert_ctx);

    return resultFrame;
}

void StreamReader::initializeAv()
{
    avdevice_register_all();
    
    m_inputFormat = av_find_input_format(getAvInputFormat());
    if (!m_inputFormat)
    {
        m_lastError.setAvError("could not find input format.");
        return;
    }
    
    m_formatContext = avformat_alloc_context();
    setOptions();

    QString url = getAvCameraUrl();
    int formatOpenCode = avformat_open_input(&m_formatContext, url.toLatin1().data(),
        m_inputFormat, &m_options);
    if (m_lastError.updateIfError(formatOpenCode))
        return;

    /*int findStreamCode = avformat_find_stream_info(m_formatContext, nullptr);
    if (m_lastError.updateIfError(findStreamCode))
        return;*/

    int videoStreamIndex;
    AVStream * videoStream = 
        utils::av::getAvStream(m_formatContext, &videoStreamIndex, AVMEDIA_TYPE_VIDEO);
    if (!videoStream)
    {
        m_lastError.setAvError("could not find a video stream");
        return;
    }

    m_videoDecoder.reset(new AVCodecContainer(m_formatContext));
    m_videoDecoder->initializeDecoder(videoStream->codecpar);
    if (!m_videoDecoder->isValid())
    {
        m_lastError.setAvError(m_videoDecoder->avErrorString());
        m_videoDecoder.reset(nullptr);
        return;
    }
    m_videoDecoder->open();

    m_videoEncoder.reset(new AVCodecContainer(m_formatContext));
    m_videoEncoder->initializeEncoder(AV_CODEC_ID_MJPEG);
    if (!m_videoEncoder->isValid())
    {
        m_lastError.setAvError(m_videoEncoder->avErrorString());
        m_videoEncoder.reset(nullptr);
        return;
    }

    m_avVideoPacket = av_packet_alloc();

    setEncoderOptions();
}

void StreamReader::unInitializeAv()
{
    if (m_avVideoPacket)
        av_packet_free(&m_avVideoPacket);

    //close the codecs before we close_input() below
    m_videoDecoder.reset(nullptr);
    m_videoEncoder.reset(nullptr);

    av_dict_free(&m_options);

    if (m_formatContext)
        avformat_close_input(&m_formatContext);
}

void StreamReader::setOptions()
{
    if(m_formatContext)
    {
        m_formatContext->flags |= AVFMT_FLAG_NOBUFFER;
        m_formatContext->flags |= AVFMT_FLAG_DISCARD_CORRUPT;
        m_formatContext->flags |= AVFMT_FLAG_FLUSH_PACKETS;
    }

    if(m_resolution.isValid())
    {
        QString video_size =
            QString("%1x%2").arg(m_resolution.width()).arg(m_resolution.height());
        av_dict_set(&m_options, "video_size", video_size.toLatin1().data(), 0);
    }

    if(m_fps)
    {
        QString fps = QString::number(m_fps);
        av_dict_set(&m_options, "framerate", fps.toLatin1().data(), 0);
    }
    
    // todo find out why this doesn't appear to have any effect
    if(m_bitrate && m_formatContext)
        m_formatContext->bit_rate = m_bitrate;

    //av_dict_set(&m_options, "rtbufsize", "2000M", 0);
}

void StreamReader::setEncoderOptions()
{
    AVCodecContext * encoderContext = m_videoEncoder->codecContext();
    AVCodecContext * decoderContext = m_videoDecoder->codecContext();

    //todo unhardcode these
    encoderContext->width = decoderContext->width;
    encoderContext->height = decoderContext->height;
    encoderContext->pix_fmt = utils::av::suggestPixelFormat(encoderContext->codec_id);
    encoderContext->time_base = { 1, m_fps > 0 ? (int)m_fps : 30}; // 30 is default framerate
    encoderContext->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
    encoderContext->global_quality = encoderContext->qmin * FF_QP2LAMBDA;

    m_videoEncoder->open();
}

const char * StreamReader::getAvInputFormat()
{
    return
#ifdef _WIN32 || _WIN64
        "dshow";
#elif __linux__ 
        "v4l2";
#elif __APPLE__
        "avfoundation";
#else
        "";
#endif
}

QString StreamReader::getAvCameraUrl()
{
    return
#ifdef _WIN32
        QString("video=").append(m_info.modelName);
#elif __linux__ 
        "Linux not implemented";
#elif __APPLE__
        "Apple not implemented";
#else
        "unsupported OS";
#endif
}

} // namespace webcam_plugin
} // namespace nx
