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

int StreamReader::getNextData(nxcip::MediaDataPacket** lpPacket)
{
    QnMutexLocker lock(&m_mutex);

    if (!isValid())
        return nxcip::NX_IO_ERROR;

    if (!ensureInitialized())
        return nxcip::NX_TRY_AGAIN;

    int nxErrorCode;
    std::unique_ptr<ILPVideoPacket> nxVideoPacket;

    if(m_transcodingNeeded)
    {
        nxVideoPacket = transcodeVideo(&nxErrorCode, m_avDecodePacket);
    }
    else
    {
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

    av_packet_unref(m_avDecodePacket);
    *lpPacket = nxErrorCode == nxcip::NX_NO_ERROR ? nxVideoPacket.release() : nullptr;
    return nxErrorCode;
}

void StreamReader::interrupt()
{
    QnMutexLocker lock(&m_mutex);
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
        m_timeProvider->millisSinceEpoch()  * USEC_IN_MS,
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
    AVFrame * decodedFrame = NULL;
    decodeVideoFrame(&decodedFrame, decodePacket);
    if (!decodedFrame)
    {
        *nxcipErrorCode = nxcip::NX_IO_ERROR;
        return nullptr;
    }

    AVFrame* convertedFrame = toEncodableFrame(decodedFrame);
    AVPacket * encodedPacket = av_packet_alloc();

    std::unique_ptr<ILPVideoPacket> nxVideoPacket = nullptr;
    int gotPacket;
    int encodeCode = m_videoEncoder->encodeVideo(encodedPacket, convertedFrame, &gotPacket);
    if (updateIfAVError(encodeCode))
    {
        *nxcipErrorCode = nxcip::NX_IO_ERROR;
    }
    else // no error
    {
        nxVideoPacket = toNxVideoPacket(encodedPacket, m_videoEncoder->codecID());
        *nxcipErrorCode = nxcip::NX_NO_ERROR;
    }

    av_frame_free(&decodedFrame);

    av_freep(&convertedFrame->data[0]);
    av_frame_free(&convertedFrame);

    av_packet_free(&encodedPacket);

    return nxVideoPacket;
}

int StreamReader::decodeVideoFrame(AVFrame**outFrame, AVPacket * decodePacket)
{
    AVFrame * decodedFrame = av_frame_alloc();
    int gotPicture = 0;
    int decodeCode = 0;
    while (!gotPicture)
    {
        decodeCode =
            m_videoDecoder->decodeVideo(m_formatContext, decodedFrame, &gotPicture, decodePacket);
        if (updateIfAVError(decodeCode))
            break;
    }

    if(gotPicture)
    {
        *outFrame = decodedFrame;
    }
    else
    {
        av_frame_free(&decodedFrame);
        *outFrame = NULL;
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

AVFrame * StreamReader::toEncodableFrame(AVFrame * frame) const
{
    AVCodecContext * decoderContext = m_videoDecoder->codecContext();

    const AVPixelFormat* supportedFormats = m_videoEncoder->codec()->pix_fmts;
    AVPixelFormat encoderFormat = supportedFormats
        ? utils::av::unDeprecatePixelFormat(supportedFormats[0])
        : utils::av::suggestPixelFormat(m_videoEncoder->codecContext()->codec_id);

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
        NX_DEBUG(this) << "openVideoEncoder();";
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

    NX_DEBUG(this) << "openInputFormat()::Camera URL:" << url << ", open code:" << formatOpenCode;

    return formatOpenCode;
}

int StreamReader::openVideoDecoder()
{
    AVStream * videoStream = utils::av::getAVStream(m_formatContext, AVMEDIA_TYPE_VIDEO);
    if (!videoStream)
        return AVERROR_STREAM_NOT_FOUND;

    auto decoder = std::make_unique<AVCodecContainer>();
    int initCode = decoder->initializeDecoder(videoStream->codecpar);

    //setDecoderOptions(decoder.get());

    NX_DEBUG(this) << "openVideoDecoder()::initCode:" << initCode;
    if (initCode < 0)
        return initCode;

    int openCode = decoder->open();
    NX_DEBUG(this) << "openVideoDecoder()::openCode:" << openCode;

    if(openCode < 0)
        return openCode;

    m_videoDecoder = std::move(decoder);
    return 0;
}

void StreamReader::setEncoderOptions(AVCodecContainer* encoder)
{
    AVCodecContext* encoderContext = encoder->codecContext();
    encoderContext->width = m_codecContext.resolution().width;
    encoderContext->height = m_codecContext.resolution().height;
    encoderContext->pix_fmt = utils::av::suggestPixelFormat(encoderContext->codec_id);
    encoderContext->framerate = { (int) m_codecContext.fps(), 1 };
    encoderContext->time_base = { 1, (int) m_codecContext.fps() };
    encoderContext->bit_rate = m_codecContext.bitrate();
    encoderContext->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
    encoderContext->global_quality = encoderContext->qmin * FF_QP2LAMBDA;
}

void StreamReader::setDecoderOptions(AVCodecContainer* decoder)
{
    AVCodecContext *decoderContext = decoder->codecContext();
    decoderContext->bit_rate = m_codecContext.bitrate();
}

int StreamReader::openVideoEncoder()
{
    auto encoder = std::make_unique<AVCodecContainer>();
    //todo compile ffmpeg with h264 support for encoding
    int initCode = encoder->initializeEncoder(AV_CODEC_ID_H263P);
    if (initCode < 0)
        return initCode;

    setEncoderOptions(encoder.get());

    int openCode = encoder->open();
    if (openCode < 0)
        return openCode;

    m_videoEncoder = std::move(encoder);
    return 0;
}

void StreamReader::uninitializeAV()
{
   if (m_avDecodePacket)
        av_packet_free(&m_avDecodePacket);

    //close the codecs before we close_input below
    m_videoDecoder.reset(nullptr);
    m_videoEncoder.reset(nullptr);

    if(m_formatContextOptions)
        av_dict_free(&m_formatContextOptions);

    if (m_formatContext)
        avformat_close_input(&m_formatContext);

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
        };

    if(!m_initialized)
    {
        initializeAV();
        NX_DEBUG(this) << "ensureInitialized(): first initialization";
        printErrNo();
    }
    else if(m_modified)
    {
        NX_DEBUG(this) << "ensureInitialized(): codec parameters modified, reinitializing";
        uninitializeAV();
        initializeAV();
        printErrNo();
        m_modified = false;
    }

    return m_initialized;
}

void StreamReader::setFormatContextOptions()
{
    nxcip::CompressionType codecID = m_codecContext.codecID();
    m_formatContext->video_codec_id = utils::av::toAVCodecID(codecID);

    NX_DEBUG(this) << "setFormatContextOptions()::AVCodecID:"
        << avcodec_get_name(m_formatContext->video_codec_id);

    m_formatContext->flags |= AVFMT_FLAG_NOBUFFER;
    m_formatContext->flags |= AVFMT_FLAG_DISCARD_CORRUPT;
    //m_formatContext->flags |= AVFMT_FLAG_FLUSH_PACKETS;

    if(m_codecContext.resolutionValid())
    {
        auto resolution = m_codecContext.resolution();
        // eg. "1920x1080"
        std::string videoSize =
            std::to_string(resolution.width).append("x").append(std::to_string(resolution.height));
        NX_DEBUG(this) << "     ffmpeg video_size:" << videoSize;
        av_dict_set(&m_formatContextOptions, "video_size", videoSize.c_str(), 0);
    }

    if(m_codecContext.fps())
    {
        std::string fps = std::to_string((int)m_codecContext.fps());
        NX_DEBUG(this) << "     ffmpeg framerate:" << fps;
        av_dict_set(&m_formatContextOptions, "framerate", fps.c_str(), 0);
    }
}

void StreamReader::setAVError(int errorCode)
{
    NX_DEBUG(this) << "AVErrorCode," << errorCode << ":" << utils::av::avStrError(errorCode);
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

std::string StreamReader::getAVCameraUrl()
{
    QString s = decodeCameraInfoUrl();
    return
#ifdef _WIN32
        std::string("video=@device_pnp_").append(s.toLatin1().data());
#elif __linux__
        std::string(s.toLatin1().data());
#elif __APPLE__
        "Apple not implemented";
#else
        "unsupported OS";
#endif
}

} // namespace webcam_plugin
} // namespace nx
