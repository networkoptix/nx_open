#include "ffmpeg_video_decoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QThread>

extern "C" {
#include <libavutil/imgutils.h>
} // extern "C"


#include "utils/media/nalUnits.h"

/* DXVA headers (should be included before ffmpeg headers). */
#   ifdef _USE_DXVA
#       include <d3d9.h>
#       include <dxva2api.h>
#       include <windows.h>
#       include <windowsx.h>
#       include <ole2.h>
#       include <commctrl.h>
#       include <shlwapi.h>
#       include <Strsafe.h>
#   endif

#ifdef _USE_DXVA
#include "dxva/ffmpeg_callbacks.h"
#endif

#include <utils/math/math.h>
#include <nx/utils/log/log.h>

#include "utils/media/frame_type_extractor.h"

#include <nx/streaming/av_codec_media_context.h>
#include <utils/media/utils.h>
#include <nx/metrics/metrics_storage.h>

static const int  LIGHT_CPU_MODE_FRAME_PERIOD = 2;
static const int MAX_DECODE_THREAD = 4;
bool QnFfmpegVideoDecoder::m_first_instance = true;
int QnFfmpegVideoDecoder::hwcounter = 0;

namespace {

    static bool isImageCanBeDecodedViaQt(AVCodecID compressionType)
    {
        switch (compressionType)
        {
            case AV_CODEC_ID_PNG:
                return true;
            default:
                return false;
        }
    }

} // namespace

// ================================================

struct FffmpegLog
{
    static void av_log_default_callback_impl(void* /*ptr*/, int /*level*/, const char* fmt, va_list vl)
    {
        NX_ASSERT(fmt && "NULL Pointer");

        if (!fmt) {
            return;
        }
        static char strText[1024 + 1];
        vsnprintf(&strText[0], sizeof(strText) - 1, fmt, vl);
        va_end(vl);

        qDebug() << "ffmpeg library: " << strText;
    }
};

QnFfmpegVideoDecoder::QnFfmpegVideoDecoder(
    const DecoderConfig& config,
    nx::metrics::Storage* metrics,
    const QnConstCompressedVideoDataPtr& data)
    :
    m_passedContext(0),
    m_context(0),
    m_codecId(data->compressionType),
    m_showmotion(false),
    m_decodeMode(DecodeMode_Full),
    m_newDecodeMode(DecodeMode_NotDefined),
    m_lightModeFrameCounter(0),
    m_frameTypeExtractor(0),
    m_deinterlaceBuffer(0),
    m_usedQtImage(false),
    m_currentWidth(-1),
    m_currentHeight(-1),
    m_checkH264ResolutionChange(false),
    m_forceSliceDecoding(-1),
    m_prevSampleAspectRatio( 1.0 ),
    m_prevTimestamp(AV_NOPTS_VALUE),
    m_spsFound(false),
    m_mtDecodingPolicy(config.mtDecodePolicy),
    m_useMtDecoding(false),
    m_needRecreate(false),
    m_metrics(metrics)
{
    if (m_metrics)
        m_metrics->decoders()++;
    setMultiThreadDecoding(m_mtDecodingPolicy == MultiThreadDecodePolicy::enabled);

    if (data->context)
    {
        m_passedContext = avcodec_alloc_context3(nullptr);
        QnFfmpegHelper::mediaContextToAvCodecContext(m_passedContext, data->context);
    }

    // XXX Debug, should be passed in constructor
    m_tryHardwareAcceleration = false; //hwcounter % 2;

    openDecoder(data);

}

QnFfmpegVideoDecoder::~QnFfmpegVideoDecoder(void)
{
    closeDecoder();

    QnFfmpegHelper::deleteAvCodecContext(m_passedContext);
    m_passedContext = 0;
    if (m_metrics)
        m_metrics->decoders()--;
}

void QnFfmpegVideoDecoder::flush()
{
    //avcodec_flush_buffers(c); // does not flushing output frames
    int got_picture = 0;
    QnFfmpegAvPacket avpkt;
    while (decodeVideo(m_context, m_frame, &got_picture, &avpkt) > 0);
}

AVCodec* QnFfmpegVideoDecoder::findCodec(AVCodecID codecId)
{
    AVCodec* codec = 0;

    if (codecId != AV_CODEC_ID_NONE)
        codec = avcodec_find_decoder(codecId);

    return codec;
}

void QnFfmpegVideoDecoder::closeDecoder()
{
    QnFfmpegHelper::deleteAvCodecContext(m_context);
    m_context = 0;
#ifdef _USE_DXVA
    m_decoderContext.close();
#endif

    av_frame_free(&m_frame);
    av_freep(&m_deinterlaceBuffer);
    av_frame_free(&m_deinterlacedFrame);
    delete m_frameTypeExtractor;
}

void QnFfmpegVideoDecoder::determineOptimalThreadType(const QnConstCompressedVideoDataPtr& data)
{
    if (m_useMtDecoding)
        m_context->thread_count = qMin(MAX_DECODE_THREAD, QThread::idealThreadCount() + 1);
    else
        m_context->thread_count = 1; //< Turn off multi thread decoding.

    NX_DEBUG(this, "Initialize video decoder, codec id: %1, thread count: %2",
        m_context->codec_id, m_context->thread_count);

    if (m_forceSliceDecoding == -1 && data && data->data() && m_context->codec_id == AV_CODEC_ID_H264)
    {
        m_forceSliceDecoding = 0;
        int nextSliceCnt = 0;
        const quint8* curNal = (const quint8*) data->data();
        if (curNal[0] == 0)
        {
            const quint8* end = curNal + data->dataSize();
            for(curNal = NALUnit::findNextNAL(curNal, end); curNal < end; curNal = NALUnit::findNextNAL(curNal, end))
            {
                quint8 nalType = *curNal & 0x1f;
                if (nalType >= nuSliceNonIDR && nalType <= nuSliceIDR) {
                    BitStreamReader bitReader;
                    try {
                        bitReader.setBuffer(curNal + 1, end);
                        int first_mb_in_slice = NALUnit::extractUEGolombCode(bitReader);
                        if (first_mb_in_slice > 0) {
                            nextSliceCnt++;
                            //break;
                        }
                    } catch(...) {
                        break;
                    }
                }
            }
            if (nextSliceCnt >= 3)
                m_forceSliceDecoding = 1; // multislice frame. Use multislice decoding if slice count 4 or above
        }
    }

    m_context->thread_type = m_context->thread_count > 1 && (m_forceSliceDecoding != 1) ? FF_THREAD_FRAME : FF_THREAD_SLICE;

    if (m_context->codec_id == AV_CODEC_ID_H264 && m_context->thread_type == FF_THREAD_SLICE)
    {
        // ignoring deblocking filter type 1 for better perfomace between H264 slices.
        m_context->flags2 |= AV_CODEC_FLAG2_FAST;
    }
    //m_context->flags |= CODEC_FLAG_GRAY;
}

void QnFfmpegVideoDecoder::openDecoder(const QnConstCompressedVideoDataPtr& data)
{
    m_codec = findCodec(m_codecId);

    m_context = avcodec_alloc_context3(m_passedContext ? 0 : m_codec);

    if (m_passedContext)
        QnFfmpegHelper::copyAvCodecContex(m_context, m_passedContext);

    m_frameTypeExtractor = new FrameTypeExtractor(QnConstMediaContextPtr(new QnAvCodecMediaContext(m_context)));

#ifdef _USE_DXVA
    if (m_codecId == AV_CODEC_ID_H264)
    {
        m_context->get_format = FFMpegCallbacks::ffmpeg_GetFormat;
        m_context->get_buffer = FFMpegCallbacks::ffmpeg_GetFrameBuf;
        m_context->release_buffer = FFMpegCallbacks::ffmpeg_ReleaseFrameBuf;

        m_context->opaque = &m_decoderContext;
    }
#endif

    m_frame = av_frame_alloc();
    m_deinterlacedFrame = av_frame_alloc();

    //if(m_codec->capabilities&CODEC_CAP_TRUNCATED)    c->flags|= CODEC_FLAG_TRUNCATED;

    //c->debug_mv = 1;
    //m_context->debug |= FF_DEBUG_THREADS + FF_DEBUG_PICT_INFO;
    //av_log_set_callback(FffmpegLog::av_log_default_callback_impl);

    determineOptimalThreadType(data);

    m_checkH264ResolutionChange = m_context->thread_count > 1 && m_context->codec_id == AV_CODEC_ID_H264 && (!m_context->extradata_size || m_context->extradata[0] == 0);

    NX_VERBOSE(this, QLatin1String("Creating ") + QLatin1String(m_context->thread_count > 1 ? "FRAME threaded decoder" : "SLICE threaded decoder"));
    // TODO: #rvasilenko check return value
    if (avcodec_open2(m_context, m_codec, NULL) < 0)
    {
        // try to reopen decoder without passed context
        if (m_passedContext)
        {
            QnFfmpegHelper::deleteAvCodecContext(m_passedContext);
            m_passedContext = nullptr;
            resetDecoder(data);
        }
    }
    //NX_ASSERT(m_context->codec);
}

void QnFfmpegVideoDecoder::resetDecoder(const QnConstCompressedVideoDataPtr& data)
{
    if (!(data->flags & AV_PKT_FLAG_KEY))
    {
        m_needRecreate = true;
        return; // can't reset right now
    }

    QnFfmpegHelper::deleteAvCodecContext(m_passedContext);
    m_passedContext = nullptr;

    if (data->context)
    {
        m_codec = findCodec(data->context->getCodecId());
        m_passedContext = avcodec_alloc_context3(nullptr);
        QnFfmpegHelper::mediaContextToAvCodecContext(m_passedContext, data->context);
    }
    if (m_passedContext && m_passedContext->width > 8 && m_passedContext->height > 8 && m_currentWidth == -1)
    {
        m_currentWidth = m_passedContext->width;
        m_currentHeight = m_passedContext->height;
    }

    // I have improved resetDecoder speed (I have left only minimum operations) because of REW. REW calls reset decoder on each GOP.
    QnFfmpegHelper::deleteAvCodecContext(m_context);
    m_context = 0;
    m_context = avcodec_alloc_context3(m_passedContext ? 0 : m_codec);

    if (m_passedContext)
        QnFfmpegHelper::copyAvCodecContex(m_context, m_passedContext);

    determineOptimalThreadType(data);
    //m_context->thread_count = qMin(5, QThread::idealThreadCount() + 1);
    //m_context->thread_type = m_mtDecoding ? FF_THREAD_FRAME : FF_THREAD_SLICE;
    // ensure that it is H.264 with nal prefixes
    m_checkH264ResolutionChange = m_context->thread_count > 1 && m_context->codec_id == AV_CODEC_ID_H264 && (!m_context->extradata_size || m_context->extradata[0] == 0);

    avcodec_open2(m_context, m_codec, NULL);

    //m_context->debug |= FF_DEBUG_THREADS;
    //m_context->flags2 |= CODEC_FLAG2_FAST;
    m_frame->data[0] = 0;
    m_spsFound = false;
}

void QnFfmpegVideoDecoder::reallocateDeinterlacedFrame()
{
    int roundWidth = qPower2Ceil((unsigned) m_context->width, 32);
    int numBytes = av_image_get_buffer_size(m_context->pix_fmt, roundWidth, m_context->height, /*align*/ 1);
    if (numBytes > 0) {
        if (m_deinterlaceBuffer)
            av_free(m_deinterlaceBuffer);

        m_deinterlaceBuffer = (quint8*)av_malloc(numBytes * sizeof(quint8));
        av_image_fill_arrays(
            m_deinterlacedFrame->data,
            m_deinterlacedFrame->linesize,
            m_deinterlaceBuffer,
            m_context->pix_fmt, roundWidth, m_context->height, /*align*/ 1);
        m_deinterlacedFrame->width = m_context->width;
        m_deinterlacedFrame->height = m_context->height;
    }
}

void QnFfmpegVideoDecoder::processNewResolutionIfChanged(const QnConstCompressedVideoDataPtr& data, int width, int height)
{
    if (m_currentWidth == -1) {
        m_currentWidth = width;
        m_currentHeight = height;
    }
    else if (width != m_currentWidth || height != m_currentHeight)
    {
        m_currentWidth = width;
        m_currentHeight = height;
        m_needRecreate = false;
        resetDecoder(data);
    }
}

void QnFfmpegVideoDecoder::setMultiThreadDecoding(bool value)
{
    if (value != m_useMtDecoding)
    {
        m_useMtDecoding = value;
        m_needRecreate = true;
    }
}

int QnFfmpegVideoDecoder::decodeVideo(
    AVCodecContext *avctx,
    AVFrame *picture,
    int *got_picture_ptr,
    const AVPacket *avpkt)
{
    m_lastDecodeResult = avcodec_decode_video2(avctx, picture, got_picture_ptr, avpkt);
    if (m_lastDecodeResult > 0 && m_metrics)
        m_metrics->decodedPixels() += picture->width * picture->height;

    return m_lastDecodeResult;
}

//The input buffer must be FF_INPUT_BUFFER_PADDING_SIZE larger than the actual read bytes because some optimized bitstream readers read 32 or 64 bits at once and could read over the end.
//The end of the input buffer buf should be set to 0 to ensure that no overreading happens for damaged MPEG streams.
bool QnFfmpegVideoDecoder::decode(const QnConstCompressedVideoDataPtr& data, QSharedPointer<CLVideoDecoderOutput>* const outFramePtr)
{
    bool isImage = false;
    if (data)
    {
        isImage = data->flags.testFlag(QnAbstractMediaData::MediaFlags_StillImage)
            || isImageCanBeDecodedViaQt(data->compressionType);
    }

    if (data && m_codecId!= data->compressionType) {
        if (m_codecId != AV_CODEC_ID_NONE && data->context)
        {
            if (!data->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
                return false; //< Can't switch decoder to new codec right now.
            resetDecoder(data);
        }
        m_codecId = data->compressionType;
    }

    CLVideoDecoderOutput* const outFrame = outFramePtr->data();
    AVFrame* copyFromFrame = m_frame;
    int got_picture = 0;

    if (data)
    {
        if (m_codec==0)
        {
            if (!isImage) {
                // try to decode in QT for still image
                NX_WARNING(this, QLatin1String("decoder not found: m_codec = 0"));
                return false;
            }
        }

        if (m_newDecodeMode != DecodeMode_NotDefined && (data->flags & AV_PKT_FLAG_KEY))
        {
            m_decodeMode = m_newDecodeMode;
            m_newDecodeMode = DecodeMode_NotDefined;
            m_lightModeFrameCounter = 0;
        }

        if ((m_decodeMode > DecodeMode_Full || (data->flags & QnAbstractMediaData::MediaFlags_Ignore)) && data->data())
        {

            if (data->compressionType == AV_CODEC_ID_MJPEG)
            {
                uint period = m_decodeMode == DecodeMode_Fast ? LIGHT_CPU_MODE_FRAME_PERIOD : LIGHT_CPU_MODE_FRAME_PERIOD*2;
                if (m_lightModeFrameCounter < period)
                {
                    ++m_lightModeFrameCounter;
                    return false;
                }
                else
                    m_lightModeFrameCounter = 0;

            }
            else if (!(data->flags & AV_PKT_FLAG_KEY))
            {
                if (m_decodeMode == DecodeMode_Fastest)
                    return false;
                else if (m_frameTypeExtractor->getFrameType((quint8*) data->data(), static_cast<int>(data->dataSize())) == FrameTypeExtractor::B_Frame)
                    return false;
            }
        }
#ifdef _USE_DXVA
        bool needResetCodec = false;

        if (m_decoderContext.isHardwareAcceleration() && !isHardwareAccellerationPossible(data->codec, data->width, data->height)
            || m_tryHardwareAcceleration && !m_decoderContext.isHardwareAcceleration() && isHardwareAccellerationPossible(data->codec, data->width, data->height))
        {
            m_decoderContext.setHardwareAcceleration(!m_decoderContext.isHardwareAcceleration());
            m_needRecreate = true;
        }
#endif

        if (m_needRecreate && (data->flags & AV_PKT_FLAG_KEY))
        {
            m_needRecreate = false;
            resetDecoder(data);
        }

        QnFfmpegAvPacket avpkt((unsigned char*) data->data(), (int) data->dataSize());
        avpkt.dts = avpkt.pts = data->timestamp;
        // HACK for CorePNG to decode as normal PNG by default
        avpkt.flags = AV_PKT_FLAG_KEY;

        // from avcodec_decode_video2() docs:
        // 1) The input buffer must be FF_INPUT_BUFFER_PADDING_SIZE larger than
        // the actual read bytes because some optimized bitstream readers read 32 or 64
        // bits at once and could read over the end.
        // 2) The end of the input buffer buf should be set to 0 to ensure that
        // no overreading happens for damaged MPEG streams.

        // 1 is already guaranteed by QnByteArray, let's apply 2 here...
        if (avpkt.data)
            memset(avpkt.data + avpkt.size, 0, FF_INPUT_BUFFER_PADDING_SIZE);

        // ### handle errors
        if (m_context->pix_fmt == -1)
            m_context->pix_fmt = AVPixelFormat(0);

        bool dataWithNalPrefixes = (m_context->extradata==0 || m_context->extradata[0] == 0);
        // workaround ffmpeg crash
        if (m_checkH264ResolutionChange && avpkt.size > 4 && dataWithNalPrefixes)
        {
            const quint8* dataEnd = avpkt.data + avpkt.size;
            const quint8* curPtr = avpkt.data;
            // New version scan whole data to find SPS unit.
            // It is slower but some cameras do not put SPS the begining of a data.
            while (curPtr < dataEnd - 2)
            {
                const int nalLen = curPtr[2] == 0x01 ? 3 : 4;
                if (curPtr + nalLen >= dataEnd)
                    break;

                auto nalUnitType = curPtr[nalLen] & 0x1f;
                if (nalUnitType == nuSPS)
                {
                    SPSUnit sps;
                    const quint8* end = NALUnit::findNALWithStartCode(curPtr+nalLen, dataEnd, true);
                    sps.decodeBuffer(curPtr + nalLen, end);
                    sps.deserialize();
                    processNewResolutionIfChanged(data, sps.getWidth(), sps.getHeight());
                    m_spsFound = true;
                    break;
                }
                curPtr = NALUnit::findNALWithStartCode(curPtr + nalLen, dataEnd, true);
            }
            if (!m_spsFound && m_context->extradata_size == 0)
                return false; // no sps has found yet. skip frame
        }
        else if (data->context)
        {
            if (data->context->getWidth() != 0 && data->context->getHeight() != 0)
                processNewResolutionIfChanged(data, data->context->getWidth(), data->context->getHeight());
        }

        // -------------------------
        if(m_context->codec)
        {
            decodeVideo(m_context, m_frame, &got_picture, &avpkt);
            for(int i = 0; i < 2 && !got_picture && (data->flags & QnAbstractMediaData::MediaFlags_DecodeTwice); ++i)
                decodeVideo(m_context, m_frame, &got_picture, &avpkt);
        }

        if (got_picture)
        {
            if (m_mtDecodingPolicy == MultiThreadDecodePolicy::autoDetect)
            {
                if (m_context->width <= 3500)
                    setMultiThreadDecoding(false);
                else
                {
                    qint64 frameDistance = data->timestamp - m_prevTimestamp;
                    if (frameDistance > 0)
                    {
                        const double fps = 1000000.0 / frameDistance;
                        qint64 megapixels = m_context->width * m_context->height * fps;
                        static const qint64 kThreshold = 1920*2 * 1080*2 * 20; //< 4k, 20fps
                        if (megapixels >= kThreshold)
                            setMultiThreadDecoding(true); // high fps and high resolution
                    }
                }
            }
            m_prevTimestamp = data->timestamp;
        }

        // sometimes ffmpeg can't decode image files. Try to decode in QT
        m_usedQtImage = false;
        if (!got_picture && isImage)
        {
            m_tmpImg.loadFromData(avpkt.data, avpkt.size);
            if (m_tmpImg.width() > 0 && m_tmpImg.height() > 0) {
                // TODO: #dklychkov Maybe set correct ffmpeg format if possible instead of conversion
                if (m_tmpImg.format() != QImage::Format_RGBA8888)
                    m_tmpImg = m_tmpImg.convertToFormat(QImage::Format_RGBA8888);

                got_picture = 1;
                m_tmpQtFrame.setUseExternalData(true);
                m_tmpQtFrame.format = AV_PIX_FMT_RGBA;
                m_tmpQtFrame.data[0] = (quint8*) m_tmpImg.constBits();
                m_tmpQtFrame.data[1] = m_tmpQtFrame.data[2] = m_tmpQtFrame.data[3] = 0;
                m_tmpQtFrame.linesize[0] = m_tmpImg.bytesPerLine();
                m_tmpQtFrame.linesize[1] = m_tmpQtFrame.linesize[2] = m_tmpQtFrame.linesize[3] = 0;
                m_context->width = m_tmpQtFrame.width = m_tmpImg.width();
                m_context->height = m_tmpQtFrame.height = m_tmpImg.height();
                m_tmpQtFrame.pkt_dts = data->timestamp;
                copyFromFrame = &m_tmpQtFrame;
                m_usedQtImage = true;
            }
        }
    }
    else {
        QnFfmpegAvPacket avpkt;
        avpkt.pts = avpkt.dts = m_prevTimestamp;
        decodeVideo(m_context, m_frame, &got_picture, &avpkt); // flush
    }

    if (got_picture)
    {
#if 0
        // todo: ffmpeg-test . implement me
        if (m_frame->interlaced_frame && m_context->thread_count > 1)
        {

            if (m_deinterlacedFrame->width != m_context->width || m_deinterlacedFrame->height != m_context->height)
                reallocateDeinterlacedFrame();

            if (outFrame->isExternalData())
            {
                if (avpicture_deinterlace((AVPicture*)m_deinterlacedFrame, (AVPicture*) m_frame, m_context->pix_fmt, m_context->width, m_context->height) == 0) {
                    m_deinterlacedFrame->pkt_dts = m_frame->pkt_dts;
                    copyFromFrame = m_deinterlacedFrame;
                }
            }
            else {
                avpicture_deinterlace((AVPicture*) outFrame, (AVPicture*) m_frame, m_context->pix_fmt, m_context->width, m_context->height);
                outFrame->pkt_dts = m_frame->pkt_dts;
            }
        }
        else
#endif
        if (!outFrame->isExternalData())
        {
            outFrame->copyDataOnlyFrom(copyFromFrame);
            // pkt_dts and pkt_pts are mixed up after decoding in ffmpeg. So, we have to use dts here instead of pts
            outFrame->pkt_dts = m_frame->pkt_dts != AV_NOPTS_VALUE ? m_frame->pkt_dts : m_frame->pkt_pts;
        }

#ifdef _USE_DXVA
        if (m_decoderContext.isHardwareAcceleration())
            m_decoderContext.extract(m_frame);
#endif

        m_context->debug_mv = 0;
        if (m_showmotion)
        {
            m_context->debug_mv = 1;
            //c->debug |=  FF_DEBUG_QP | FF_DEBUG_SKIP | FF_DEBUG_MB_TYPE | FF_DEBUG_VIS_QP | FF_DEBUG_VIS_MB_TYPE;
            //c->debug |=  FF_DEBUG_VIS_MB_TYPE;
            //ff_print_debug_info((MpegEncContext*)(c->priv_data), picture);
        }

        if (outFrame->isExternalData())
        {
            outFrame->width = m_context->width;
            outFrame->height = m_context->height;

            outFrame->data[0] = copyFromFrame->data[0];
            outFrame->data[1] = copyFromFrame->data[1];
            outFrame->data[2] = copyFromFrame->data[2];

            outFrame->linesize[0] = copyFromFrame->linesize[0];
            outFrame->linesize[1] = copyFromFrame->linesize[1];
            outFrame->linesize[2] = copyFromFrame->linesize[2];
            outFrame->pkt_dts = copyFromFrame->pkt_dts;
        }
        outFrame->format = GetPixelFormat();
        outFrame->fillRightEdge();
        outFrame->sample_aspect_ratio = getSampleAspectRatio();
        return m_context->pix_fmt != AV_PIX_FMT_NONE;
    }
    return false; // no picture decoded at current step
}

double QnFfmpegVideoDecoder::getSampleAspectRatio() const
{
    if (!m_context || m_context->width <= 8 || m_context->height <= 8)
        return m_prevSampleAspectRatio;

    double result = av_q2d(m_context->sample_aspect_ratio);
    if (qAbs(result) >= 1e-7)
    {
        m_prevSampleAspectRatio = result;
        return result;
    }

    QSize srcSize(m_context->width, m_context->height);
    m_prevSampleAspectRatio = nx::media::getDefaultSampleAspectRatio(srcSize);
    return m_prevSampleAspectRatio;
}

AVPixelFormat QnFfmpegVideoDecoder::GetPixelFormat() const
{
    if (m_usedQtImage)
        return AV_PIX_FMT_RGBA;

    return CLVideoDecoderOutput::fixDeprecatedPixelFormat(m_context->pix_fmt);
}

MemoryType QnFfmpegVideoDecoder::targetMemoryType() const
{
    return MemoryType::SystemMemory;
}

void QnFfmpegVideoDecoder::setLightCpuMode(QnAbstractVideoDecoder::DecodeMode val)
{
    if (m_decodeMode == val)
        return;
    if (val >= m_decodeMode || m_decodeMode < DecodeMode_Fastest)
    {
        m_decodeMode = val;
        m_newDecodeMode = DecodeMode_NotDefined;
        m_lightModeFrameCounter = 0;
    }
    else
    {
        m_newDecodeMode = val;
    }
}

void QnFfmpegVideoDecoder::showMotion(bool show)
{
        m_showmotion = show;
}

AVCodecContext* QnFfmpegVideoDecoder::getContext() const
{
    return m_context;
}

void QnFfmpegVideoDecoder::setMultiThreadDecodePolicy(MultiThreadDecodePolicy value)
{
    if (m_mtDecodingPolicy != value)
    {
        NX_VERBOSE(this, lm("Use multithread decoding policy %1").arg((int) value));
        m_mtDecodingPolicy = value;
        setMultiThreadDecoding(m_mtDecodingPolicy == MultiThreadDecodePolicy::enabled);
    }
}

#endif // ENABLE_DATA_PROVIDERS
