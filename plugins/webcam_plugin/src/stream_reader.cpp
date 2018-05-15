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

#include "av_utils.h"
#include "dshow_utils.h"
#include "avcodec_container.h"
#include "ilp_empty_packet.h"
#include "motion_data_picture.h"
#include "plugin.h"

namespace {

    static const nxcip::UsecUTCTimestamp USEC_IN_MS = 1000;
    static const nxcip::UsecUTCTimestamp USEC_IN_SEC = 1000*1000;
    static const nxcip::UsecUTCTimestamp NSEC_IN_USEC = 1000;
    static const int MAX_FRAME_SIZE = 4*1024*1024;
}

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
    m_videoEncoder(nullptr),
    m_avVideoPacket(nullptr)
{
    NX_ASSERT(m_timeProvider);
    setFps( fps );
    initializeAv();
}

StreamReader::~StreamReader()
{
    NX_ASSERT( m_isInGetNextData == 0 );
    m_timeProvider->releaseRef();

    if(m_avVideoPacket)
        av_packet_free(&m_avVideoPacket);
    
    av_dict_free(&m_options);

    if(m_formatContext)
        avformat_close_input(&m_formatContext);
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
    m_frameDurationMSec = (qint64)(1000.0 / m_fps);
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

AVPacket * StreamReader::getEncodedPacket(AVFrame * frame)
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

bool StreamReader::isValid()
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
        NULL,
        codecContext->width, codecContext->height,
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
    
    const char * inputFormat = getAvInputFormat();
    m_inputFormat = av_find_input_format(inputFormat);
    if (!m_inputFormat)
    {
        m_lastError.setAvError("could not find input format.");
        return;
    }

    //todo get the url from the m_info
    
    QString url = getAvCameraUrl();

    m_formatContext = avformat_alloc_context();
    setOptions();

    int formatOpenCode = avformat_open_input(&m_formatContext, url.toLatin1().data(), m_inputFormat, &m_options);
    if (m_lastError.updateIfError(formatOpenCode))
        return;

    int findStreamCode = avformat_find_stream_info(m_formatContext, nullptr);
    if (m_lastError.updateIfError(findStreamCode))
        return;

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

void StreamReader::setOptions()
{
    //m_formatContext->video_codec_id = AV_CODEC_ID_MJPEG;
    m_formatContext->video_codec_id = AV_CODEC_ID_H264;

    //todo actually fill out m_options instead of hardcoding
    av_dict_set(&m_options, "video_size", "1920x1080", 0);
    
    QString fps = QString::number(m_fps);
    av_dict_set(&m_options, "framerate", fps.toLatin1().data(), 0);
    
    //todo find out why this delays the realtime buffer error and fix the error
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
    encoderContext->time_base = { 1, (int)m_fps };
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
