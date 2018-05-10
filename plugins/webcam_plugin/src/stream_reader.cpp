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

extern "C"
{
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}

#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <memory>

#include <QtCore/QElapsedTimer>
#include <QtCore/QThread>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/byte_stream/custom_output_stream.h>

#include "avcodec_container.h"
#include "ilp_empty_packet.h"
#include "motion_data_picture.h"
#include "plugin.h"

AVStream* getAvStream(AVFormatContext * context, int * streamIndex, enum AVMediaType mediaType)
{
    for (unsigned int i = 0; i < context->nb_streams; ++i)
    {
        if (context->streams[i]->codecpar->codec_type == mediaType)
        {
            *streamIndex = i;
            return context->streams[i];
        }
    }

    return nullptr;
}

static const nxcip::UsecUTCTimestamp USEC_IN_MS = 1000;
static const nxcip::UsecUTCTimestamp USEC_IN_SEC = 1000*1000;
static const nxcip::UsecUTCTimestamp NSEC_IN_USEC = 1000;
static const int MAX_FRAME_SIZE = 4*1024*1024;

StreamReader::StreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    nxpl::TimeProvider *const timeProvider,
    const nxcip::CameraInfo& cameraInfo,
    float fps,
    int encoderNumber )
:
    m_refManager( parentRefManager ),
    m_info( cameraInfo ),
    m_fps( fps ),
    m_encoderNumber( encoderNumber ),
    m_curTimestamp( 0 ),
    m_streamType( none ),
    m_prevFrameClock( -1 ),
    m_frameDurationMSec( 0 ),
    m_terminated( false ),
    m_isInGetNextData( 0 ),
    m_timeProvider(timeProvider),
    m_formatContext(nullptr),
    m_inputFormat(nullptr),
    m_options(nullptr),
    m_videoDecoder(nullptr),
    m_videoEncoder(nullptr)
{
    NX_ASSERT(m_timeProvider);
    setFps( fps );
    initializeAv();
}

StreamReader::~StreamReader()
{
    NX_ASSERT( m_isInGetNextData == 0 );
    m_timeProvider->releaseRef();

    //m_videoPacket.reset();
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

int StreamReader::getNextData( nxcip::MediaDataPacket** lpPacket )
{
    if (!isValid())
        return nxcip::NX_IO_ERROR;

    AVFrame* decodedFrame = getDecodedFrame();
    if (!decodedFrame)
        return nxcip::NX_NETWORK_ERROR;

    AVFrame* convertedFrame = toYUV420(m_videoDecoder->getCodecContext(), decodedFrame);

    AVPacket * encodedPacket = av_packet_alloc();
    av_init_packet(encodedPacket);
    encodedPacket->data = NULL;
    encodedPacket->size = 0;

    int gotPacket;
    m_videoEncoder->encode(encodedPacket, convertedFrame, &gotPacket);

    writeToVideoPacket(encodedPacket);
    *lpPacket = m_videoPacket.release();

    av_frame_free(&decodedFrame);
    av_frame_free(&convertedFrame);
    av_free_packet(encodedPacket);

    return nxcip::NX_NETWORK_ERROR;
}

void StreamReader::interrupt()
{
    std::shared_ptr<nx::network::http::HttpClient> client;
    {
        QnMutexLocker lk(&m_mutex);
        m_terminated = true;
        m_cond.wakeAll();
        //std::swap(client, m_httpClient);
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
    m_frameDurationMSec = (qint64)(1000.0 / m_fps);
}

void StreamReader::updateCameraInfo( const nxcip::CameraInfo& info )
{
    m_info = info;
}

void StreamReader::writeToVideoPacket(AVPacket * packet)
{
     m_videoPacket.reset( new ILPVideoPacket(
        &m_allocator,
        0,
        m_timeProvider->millisSinceEpoch()  * USEC_IN_MS,
        nxcip::MediaDataPacket::fKeyPacket,
        0 ) );

     m_videoPacket->resizeBuffer(packet->size);
     if (m_videoPacket->data())
         memcpy(m_videoPacket->data(), packet->data, packet->size);
}


AVFrame * StreamReader::getDecodedFrame()
{
    AVFrame * decodedFrame = av_frame_alloc();
    AVPacket * packet = av_packet_alloc();
    //av_init_packet(packet);

    bool isReadCode;
    int gotPicture = 0;

    while (!gotPicture)
    {
        int returnCode = m_videoDecoder->readAndDecode(decodedFrame, &gotPicture, packet, &isReadCode);
        if (m_lastError.updateIfError(returnCode))
        {
            av_packet_free(&packet);
            return nullptr;
        }
    }

    av_packet_free(&packet);
    return decodedFrame;
}

AVPacket * StreamReader::getEncodedPacket(AVFrame * frame)
{
    AVPacket* packet = av_packet_alloc();
    av_init_packet(packet);
    packet->data = NULL;
    packet->size = 0;

    int gotPacket;
    int encodeCode = m_videoEncoder->encode(packet, frame, &gotPacket);
    
    if (encodeCode < 0 || !gotPacket)
    {
        av_packet_free(&packet);
        return nullptr;
    }

    return packet;
}

bool StreamReader::isValid()
{
    return m_formatContext != nullptr
        && m_inputFormat != nullptr
        && m_videoDecoder != nullptr 
        && m_videoEncoder != nullptr 
        && !m_lastError.hasError();
}

AVFrame * StreamReader::toYUV420(AVCodecContext * codecContext, AVFrame * frame)
{
    AVFrame* resultFrame = av_frame_alloc();

    AVPixelFormat pix_fmt = AV_PIX_FMT_YUV420P;

    av_image_alloc(resultFrame->data, resultFrame->linesize, frame->width, frame->height,
        pix_fmt, 32);

    struct SwsContext * img_convert_ctx = sws_getCachedContext(
        NULL,
        codecContext->width, codecContext->height, codecContext->pix_fmt, /*source*/
        codecContext->width, codecContext->height, pix_fmt, /*destination*/
        SWS_BICUBIC, NULL, NULL, NULL);

    sws_scale(img_convert_ctx, frame->data, frame->linesize, 0, codecContext->height,
        resultFrame->data, resultFrame->linesize);

    sws_freeContext(img_convert_ctx);

    return resultFrame;
}

static int init = 0;

void StreamReader::initializeAv()
{
    if (init)
        return;

    ++init;

    setOptions();
    avdevice_register_all();

    const char * inputFormat = getAvInputFormat();
    m_inputFormat = av_find_input_format(inputFormat);
    if (!m_inputFormat)
    {
        m_lastError.setAvError("could not find input format.");
        return;
    }

    //todo actually fill out m_options
    const char * url = "video=HD Pro Webcam C920";
    int formatOpenCode = avformat_open_input(&m_formatContext, url, m_inputFormat, &m_options);
    if (m_lastError.updateIfError(formatOpenCode))
        return;

    int findStreamCode = avformat_find_stream_info(m_formatContext, nullptr);
    if (m_lastError.updateIfError(findStreamCode))
        return;

    int videoStreamIndex;
    AVStream * videoStream = getAvStream(m_formatContext, &videoStreamIndex, AVMEDIA_TYPE_VIDEO);
    if (!videoStream)
    {
        m_lastError.setAvError("could not find a video stream");
        return;
    }

    m_videoDecoder.reset(new AVCodecContainer(m_formatContext, videoStream, false));
    if (!m_videoDecoder->isValid())
    {
        m_lastError.setAvError(m_videoDecoder->getAvError());
        m_videoDecoder.reset(nullptr);
        return;
    }

    m_videoEncoder.reset(new AVCodecContainer(m_formatContext, AV_CODEC_ID_MJPEG, true));
    if (!m_videoDecoder->isValid())
    {
        m_lastError.setAvError(m_videoDecoder->getAvError());
        m_videoDecoder.reset(nullptr);
        return;
    }

    setEncoderOptions();
}

void StreamReader::setOptions()
{
    av_dict_set(&m_options, "video_size", "1920x1080", 0);
    av_dict_set(&m_options, "frame_rate", "30", 0);
    av_dict_set(&m_options, "rtbufsize", "2000M", 0);
}

void StreamReader::setEncoderOptions()
{
    AVCodecContext * pOCodecCtx = m_videoEncoder->getCodecContext();
    pOCodecCtx->bit_rate = m_videoDecoder->getCodecContext()->bit_rate;
    
    //todo unhardcode these
    pOCodecCtx->width = 1920;
    pOCodecCtx->height = 1080;
    pOCodecCtx->codec_id = AV_CODEC_ID_MJPEG;
    pOCodecCtx->pix_fmt = pOCodecCtx->codec_id == AV_CODEC_ID_MJPEG ? AV_PIX_FMT_YUVJ420P : AV_PIX_FMT_YUV420P;
    pOCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    pOCodecCtx->time_base.num = 1;
    pOCodecCtx->time_base.den = 25;
    pOCodecCtx->thread_count = 1;
    pOCodecCtx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
    pOCodecCtx->global_quality = pOCodecCtx->qmin * FF_QP2LAMBDA;

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
                                                                                                  
bool StreamReader::waitForNextFrameTime()
{
    const qint64 currentTime = m_timeProvider->millisSinceEpoch();
    if( m_prevFrameClock != -1 &&
        !((m_prevFrameClock > currentTime) || (currentTime - m_prevFrameClock > m_frameDurationMSec)) ) //system time changed
    {
        const qint64 msecToSleep = m_frameDurationMSec - (currentTime - m_prevFrameClock);
        if( msecToSleep > 0 )
        {
            QnMutexLocker lk( &m_mutex );
            QElapsedTimer monotonicTimer;
            monotonicTimer.start();
            qint64 msElapsed = monotonicTimer.elapsed();
            while( !m_terminated && (msElapsed < msecToSleep) )
            {
                m_cond.wait( lk.mutex(), msecToSleep - msElapsed );
                msElapsed = monotonicTimer.elapsed();
            }
            if( m_terminated )
            {
                //call has been interrupted
                m_terminated = false;
                return false;
            }
        }
    }
    m_prevFrameClock = m_timeProvider->millisSinceEpoch();
    return true;
}
