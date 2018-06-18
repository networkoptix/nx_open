#include "stream_reader.h"

#ifdef _WIN32
#include <Windows.h>
#undef max
#undef min
#else
#include <time.h>
#include <unistd.h>
#include <errno.h>
#endif

#include <sys/timeb.h>
#include <memory>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
} // extern "C"

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "av_utils.h"
#include "avcodec_container.h"
#include "plugin.h"
#include "logging.h"

namespace {
static constexpr nxcip::UsecUTCTimestamp USEC_IN_MS = 1000;
}

namespace nx {
namespace webcam_plugin {

StreamReader::StreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    nxpl::TimeProvider *const timeProvider,
    const nxcip::CameraInfo& cameraInfo,
    int encoderNumber,
    const CodecContext& codecContext)
:
    m_refManager(parentRefManager),
    m_timeProvider(timeProvider),
    m_info(cameraInfo),
    m_encoderNumber(encoderNumber),
    m_codecContext(codecContext),
    m_initialized(false),
    m_modified(false),
    m_terminated(false),
    m_transcodingNeeded(m_codecContext.codecID() == nxcip::AV_CODEC_ID_NONE),
    m_formatContext( nullptr ),
    m_inputFormat(nullptr),
    m_formatContextOptions(nullptr),
    m_avDecodePacket(nullptr),
    m_videoEncoder(nullptr),
    m_videoDecoder(nullptr)
{
    NX_ASSERT(m_timeProvider);
    debugln(__FUNCTION__);
    debug("\ttranscoding needed: %s\n", m_transcodingNeeded ? "yes" : "no");
}

StreamReader::~StreamReader()
{
    m_timeProvider->releaseRef();
    uninitializeAV();
}

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

unsigned int StreamReader::addRef()
{
    return m_refManager.addRef();
}

unsigned int StreamReader::releaseRef()
{
    return m_refManager.releaseRef();
}

int count = 0;
int StreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    //QnMutexLocker lock(&m_mutex);
    //debug("%s, %d\n", __FUNCTION__, count++);
    if (!isValid())
    {
        debugln("nxcip::NX_IO_ERROR");
        return nxcip::NX_IO_ERROR;
    }

    if (!ensureInitialized())
    {
        debugln("nxcip::NX_TRY_AGAIN");
        return nxcip::NX_TRY_AGAIN;
    }

    debugln("init m_encodedPacket");
    av_init_packet(m_encodedPacket);
    debugln("finished init");

    int nxErrorCode;
    std::unique_ptr<ILPVideoPacket> nxVideoPacket = nullptr;

    // int gop_size = m_videoEncoder 
    //     ? m_videoEncoder->codecContext()->gop_size 
    //     : m_videoDecoder->codecContext()->gop_size;
    // if(count++ >= gop_size)
    // {
    //     count = 0;
    //     debugln("sending I Frame packet");
    //     AVCodecID codecID = m_videoEncoder ? m_videoEncoder->codecID() : m_videoDecoder->codecID();
    //     nxVideoPacket = toNxVideoPacket(m_cachedIframePacket, codecID);
    //     nxErrorCode = nxcip::NX_NO_ERROR;
    // }
    // else 
    if(m_transcodingNeeded)
    {
        debugln("transcoding");
        nxVideoPacket = transcodeVideo(&nxErrorCode, m_avDecodePacket);
        av_packet_unref(m_encodedPacket);
    }
    else
    {
        debugln("no transcoding needed");
        int readCode = AVCodecContainer::readFrame(m_formatContext, m_avDecodePacket);
        if (updateIfAVError(readCode))
        {
            nxErrorCode = nxcip::NX_TRY_AGAIN;
        }
        else // no errors
        {
            nxVideoPacket = toNxVideoPacket(m_avDecodePacket, m_videoDecoder->codecID());
            nxErrorCode = nxcip::NX_NO_ERROR;
        }
    }

    debug("packet size: %d\n", nxVideoPacket->dataSize());

    av_packet_unref(m_encodedPacket);
    av_packet_unref(m_avDecodePacket);
    *lpPacket = nxErrorCode == nxcip::NX_NO_ERROR ? nxVideoPacket.release() : nullptr;

    //std::string str = nxErrorCode == nxcip::NX_NO_ERROR ? "no error" : "error";
    //debugln(str.c_str());

    return nxErrorCode;
}

void StreamReader::interrupt()
{
    QnMutexLocker lk(&m_mutex);
    m_terminated = true;
}

void StreamReader::setFps( float fps )
{
    if (m_codecContext.fps() != fps)
    {
        m_codecContext.setFps(fps);
        m_modified = true;
    }
}

void StreamReader::setResolution(const nxcip::Resolution& resolution)
{
    auto currentRes = m_codecContext.resolution();
    if(currentRes.width != resolution.width
        && currentRes.height != resolution.height)
    {
        m_codecContext.setResolution(resolution);
        m_modified = true;
    }
}

void StreamReader::setBitrate(int64_t bitrate)
{
    if(m_codecContext.bitrate() != bitrate)
    {
        m_codecContext.setBitrate(bitrate);
        m_modified = true;
    }
}

void StreamReader::updateCameraInfo( const nxcip::CameraInfo& info )
{
    m_info = info;
}

bool StreamReader::isNative() const
{
    return !m_transcodingNeeded;
}

std::unique_ptr<ILPVideoPacket> StreamReader::toNxVideoPacket(AVPacket *packet, AVCodecID codecID)
{
    std::unique_ptr<ILPVideoPacket> nxVideoPacket(new ILPVideoPacket(
        &m_allocator,
        0,
        m_timeProvider->millisSinceEpoch() * USEC_IN_MS,
        nxcip::MediaDataPacket::fKeyPacket,
        0 ) );

     nxVideoPacket->resizeBuffer(packet->size);
     if (nxVideoPacket->data())
         memcpy(nxVideoPacket->data(), packet->data, packet->size);

     nxVideoPacket->setCodecType(utils::av::toNxCompressionType(codecID));

     return nxVideoPacket;
}

std::unique_ptr<ILPVideoPacket> StreamReader::transcodeVideo(
    int * nxcipErrorCode,
    AVPacket * decodePacket)
{
    int decodeCode = decodeVideoFrame(decodePacket, m_decodedFrame);
    if (updateIfAVError(decodeCode))
    {
        *nxcipErrorCode = nxcip::NX_IO_ERROR;
        return nullptr;
    }

    std::unique_ptr<ILPVideoPacket> nxVideoPacket = nullptr;
    int gotPacket;
    int encodeCode = m_videoEncoder->encodeVideo(m_encodedPacket, m_decodedFrame, &gotPacket);
    if (updateIfAVError(encodeCode))
    {
        *nxcipErrorCode = nxcip::NX_IO_ERROR;
    }
    else // no error
    {
        nxVideoPacket = toNxVideoPacket(m_encodedPacket, m_videoEncoder->codecID());
        *nxcipErrorCode = nxcip::NX_NO_ERROR;
    }

    return nxVideoPacket;
}

int StreamReader::cacheEncodedIFrame()
{
    debugln(__FUNCTION__);
    
    m_cachedIframePacket = av_packet_alloc();
    if(!m_cachedIframePacket)
        return AVERROR(ENOMEM);

    int copyCode;
    if(m_transcodingNeeded)
    {
        av_init_packet(m_encodedPacket);   
        int errorCode;
        transcodeVideo(&errorCode, m_avDecodePacket);
        copyCode = av_copy_packet(m_cachedIframePacket, m_encodedPacket);
        av_packet_unref(m_encodedPacket);
    }
    else
    {
        AVCodecContainer::readFrame(m_formatContext, m_avDecodePacket);
        copyCode = av_copy_packet(m_cachedIframePacket, m_avDecodePacket);
        debugln("done caching");
    }

    av_packet_unref(m_avDecodePacket);

    return copyCode;
}

int StreamReader::decodeVideoFrame(AVPacket * decodePacket, AVFrame* outFrame)
{
    int gotPicture = 0;
    int decodeCode = 0;
    while (!gotPicture)
    {
        decodeCode =
            m_videoDecoder->decodeVideo(m_formatContext, outFrame, &gotPicture, decodePacket);
        if (updateIfAVError(decodeCode))
            break;
    }

    if(!gotPicture)
    {
         av_frame_free(&outFrame);
    }

    return decodeCode;
}

QString StreamReader::decodeCameraInfoUrl() const
{
    QString url = QString(m_info.url).mid(9);
    return nx::utils::Url::fromPercentEncoding(url.toLatin1());
}


bool StreamReader::isValid() const
{
    return m_lastAVError >= 0;
}

/*!
 * note! if the format of the input frame matches the supported pixel format of the encoder,
 *       this function returns the in frame!. if not, a new frame is allocated in the correct format
 *       and the contents of the input frame are copied to it. the caller is responsible for freeing
 *       the allocated frame, but check that it is different from the input frame before freeing both 
 *       frames!
 * */
AVFrame * StreamReader::toEncodableFrame(AVFrame * frame) const
{
    AVCodecContext * decoderContext = m_videoDecoder->codecContext();

    const AVPixelFormat* supportedFormats = m_videoEncoder->codec()->pix_fmts;
    AVPixelFormat encoderFormat = supportedFormats
        ? utils::av::unDeprecatePixelFormat(supportedFormats[0])
        : utils::av::suggestPixelFormat(m_videoEncoder->codecContext()->codec_id);

    if(encoderFormat == frame->format)
        return frame;

    AVFrame* resultFrame = av_frame_alloc();

    av_image_alloc(resultFrame->data, resultFrame->linesize, frame->width, frame->height,
        encoderFormat, 32);

    struct SwsContext * m_imageConvertContext = sws_getCachedContext(
        NULL, frame->width, frame->height,
        utils::av::unDeprecatePixelFormat(decoderContext->pix_fmt),
        decoderContext->width, decoderContext->height, encoderFormat,
        SWS_BICUBIC, NULL, NULL, NULL);

    sws_scale(m_imageConvertContext, frame->data, frame->linesize, 0, decoderContext->height,
        resultFrame->data, resultFrame->linesize);

    // ffmpeg prints warnings if these fields are not explicitly set
    resultFrame->width = frame->width;
    resultFrame->height = frame->height;
    resultFrame->format = encoderFormat;

    sws_freeContext(m_imageConvertContext);

    return resultFrame;
}

void StreamReader::initializeAV()
{
    debugln(__FUNCTION__);
    if (m_initialized)
        return;

    int openCode = openInputFormat();
    if(updateIfAVError(openCode))
        return;

    openCode = openVideoDecoder();
    if(updateIfAVError(openCode))
        return;

    if(m_transcodingNeeded)
    {
        openCode = openVideoEncoder();
        if(updateIfAVError(openCode))
            return;
    }

    m_avDecodePacket = av_packet_alloc();
    if(!m_avDecodePacket)
    {
        setAVError(AVERROR(ENOMEM));
        return;
    }

    m_encodedPacket = av_packet_alloc();
    if(!m_encodedPacket)
    {
        setAVError(AVERROR(ENOMEM));
        return;
    }

    m_decodedFrame = av_frame_alloc();
    if(!m_decodedFrame)
    {
        setAVError(AVERROR(ENOMEM));
        return;
    }

    int cacheCode = cacheEncodedIFrame();
    if(updateIfAVError(cacheCode))
        return;

    debug("%s complete\n", __FUNCTION__);
    av_dump_format(m_formatContext, 0, getAVCameraUrl().c_str(), 0);
    m_initialized = true;
}

int StreamReader::openInputFormat()
{
    m_inputFormat = av_find_input_format(getAVInputFormat());
    if (!m_inputFormat)
        // there is no error code for Format not found
        return AVERROR_PROTOCOL_NOT_FOUND;

    m_formatContext = avformat_alloc_context();
    if (!m_formatContext)
        return AVERROR(ENOMEM);

    setFormatContextOptions();

    std::string url = getAVCameraUrl();
    int formatOpenCode = avformat_open_input(
        &m_formatContext, url.c_str(), m_inputFormat, &m_formatContextOptions);

    int count = av_dict_count(m_formatContextOptions);
    debug("unused format options count: %d\n", count);

    NX_DEBUG(this) << "openInputFormat()::Camera URL:" << url << ", open code:" << formatOpenCode;

    return formatOpenCode;
}

int StreamReader::openVideoDecoder()
{
    debugln(__FUNCTION__);

    auto decoder = std::make_unique<AVCodecContainer>();
    int initCode;
    // if(m_formatContext->video_codec_id == AV_CODEC_ID_H264 
    //     && avcodec_find_decoder_by_name("h264_mmal"))
    // {
    //     initCode = decoder->initializeDecoder("h264_mmal");
    // }
    // else
    {
        AVStream * videoStream = utils::av::getAVStream(m_formatContext, AVMEDIA_TYPE_VIDEO);
        if (!videoStream)
            return AVERROR_STREAM_NOT_FOUND;
        initCode = decoder->initializeDecoder(videoStream->codecpar);
        debug("decoder used: %s\n", decoder->codec()->long_name);
    }

    NX_DEBUG(this) << "openVideoDecoder()::initCode:" << initCode;
    if (initCode < 0)
        return initCode;

    setDecoderOptions(decoder.get());

    int openCode = decoder->open();
    NX_DEBUG(this) << "openVideoDecoder()::openCode:" << openCode;

    debug("decoder name: %s\n", decoder->codec()->long_name);

    if(openCode < 0)
        return openCode;

    m_videoDecoder = std::move(decoder);
    return 0;
}

void StreamReader::setEncoderOptions(AVCodecContainer * encoder)
{
    debugln(__FUNCTION__);

    //encoder->addOption("zerocopy", "0");

    AVCodecContext* encoderContext = encoder->codecContext();
    encoderContext->width = m_codecContext.resolution().width;
    encoderContext->height = m_codecContext.resolution().height;
    encoderContext->time_base = { 1, (int) m_codecContext.fps() };
    encoderContext->framerate = { (int) m_codecContext.fps(), 1 };
    encoderContext->bit_rate = m_codecContext.bitrate();

    encoderContext->pix_fmt = utils::av::suggestPixelFormat(encoderContext->codec_id);
    encoderContext->flags = AV_CODEC_FLAG2_LOCAL_HEADER;
    encoderContext->global_quality = encoderContext->qmin * FF_QP2LAMBDA;

    debug("    gop_size: %d\n", encoderContext->gop_size);
}

void StreamReader::setDecoderOptions(AVCodecContainer * decoder)
{
    AVCodecContext* decoderContext = decoder->codecContext();
    decoderContext->bit_rate = m_codecContext.bitrate();
}

int StreamReader::openVideoEncoder()
{
    NX_DEBUG(this) << "openVideoEncoder()";

    auto encoder = std::make_unique<AVCodecContainer>();
   
    int initCode = encoder->initializeEncoder("h264_omx");
    if (initCode < 0)
        return initCode;

    setEncoderOptions(encoder.get());

    int openCode = encoder->open();
    if (openCode < 0)
        return openCode;

    int count = av_dict_count(encoder->options());

    m_videoEncoder = std::move(encoder);
    return 0;
}

void StreamReader::uninitializeAV()
{
    debugln(__FUNCTION__);
    if (m_avDecodePacket)
        av_packet_free(&m_avDecodePacket);

    if(m_encodedPacket)
        av_packet_free(&m_encodedPacket);

    if(m_cachedIframePacket)
        av_packet_free(&m_cachedIframePacket);

    if(m_decodedFrame)
        av_frame_free(&m_decodedFrame);

    //close the codecs before we close_input below
    m_videoDecoder.reset(nullptr);
    m_videoEncoder.reset(nullptr);

    if(m_formatContextOptions)
        av_dict_free(&m_formatContextOptions);

    if (m_formatContext)
    {
        debugln("   closing input");
        avformat_close_input(&m_formatContext);
    }

    m_initialized = false;
}

bool StreamReader::ensureInitialized()
{
    auto printErrNo =
        [this]()
        {
            if(isValid())
                return;

            int err = errno;
            NX_DEBUG(this) << "ensureInitialized()::errno: " << err;
            NX_DEBUG(this) << "ensureInitialized()::m_lastAvError" << m_lastAVError;
            debug("ffmpeg error %d\n", m_lastAVError);
        };

    if(!m_initialized)
    {
        debugln("ensureInitialized first init");
        initializeAV();
        NX_DEBUG(this) << "ensureInitialized(): first initialization";
        printErrNo();
    }
    else if(m_modified)
    {
        NX_DEBUG(this) << "ensureInitialized(): codec parameters modified, reinitializing";
        debug("ensureInitialized codec params changed, re-init\n");
        uninitializeAV();
        initializeAV();
        printErrNo();
        m_modified = false;
    }

    return m_initialized;
}

void StreamReader::setFormatContextOptions()
{
    debugln(__FUNCTION__);

    nxcip::CompressionType codecID = m_codecContext.codecID();
    m_formatContext->video_codec_id = utils::av::toAVCodecID(codecID);

    NX_DEBUG(this) << "    AVCodecID:" << utils::av::avCodecIDStr(m_formatContext->video_codec_id);
    debug("    AVCodecID:%s\n", utils::av::avCodecIDStr(m_formatContext->video_codec_id).c_str());

    // m_formatContext->flags |= AVFMT_FLAG_NOBUFFER;
    // m_formatContext->flags |= AVFMT_FLAG_DISCARD_CORRUPT;

    NX_DEBUG(this) << "codecContext: " << m_codecContext.toString().c_str();
    debug("    %s\n", m_codecContext.toString().c_str());

    if(m_codecContext.resolutionValid())
    {
        // eg. "1920x1080"
        std::string videoSize = 
            std::to_string(m_codecContext.resolution().width) + "x" +
            std::to_string(m_codecContext.resolution().height);
        NX_DEBUG(this) << "ffmpeg video_size: " << videoSize;
        av_dict_set(&m_formatContextOptions, "video_size", videoSize.c_str(), 0);
    }

    if(m_codecContext.fps())
    {
        std::string fps = std::to_string((int)m_codecContext.fps());
        NX_DEBUG(this) << "ffmpeg framerate:" << fps;
        av_dict_set(&m_formatContextOptions, "framerate", fps.c_str(), 0);
    }

    //force h264 mmal decoder to be used
    av_dict_set(&m_formatContextOptions, "vcodec", "h264_mmal", 0);
}

void StreamReader::setAVError(int errorCode)
{
    NX_DEBUG(this) << "AVErrorCode," << errorCode << ":" << utils::av::avStrError(errorCode);
    debug("AVErrorCode, %d: %s\n", errorCode, utils::av::avStrError(errorCode).c_str());
    m_lastAVError = errorCode;
}

bool StreamReader::updateIfAVError(int errorCode)
{
    bool error = errorCode < 0;
    if (error)
        setAVError(errorCode);
    return error;
}

const char * StreamReader::getAVInputFormat()
{
    return "v4l2";
}

std::string StreamReader::getAVCameraUrl()
{
    QString s = decodeCameraInfoUrl();
    return std::string(s.toLatin1().data());
}

} // namespace webcam_plugin
} // namespace nx
