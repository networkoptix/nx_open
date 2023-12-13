// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_video_decoder.h"

#include <QtCore/QThread>

extern "C"
{
#include <libavutil/imgutils.h>
} // extern "C"

#include <nx/codec/nal_units.h>
#include <nx/media/codec_parameters.h>
#include <nx/media/utils.h>
#include <nx/metrics/metrics_storage.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/math.h>
#include <utils/media/frame_type_extractor.h>

static const int LIGHT_CPU_MODE_FRAME_PERIOD = 2;
static const int MAX_DECODE_THREAD = 4;

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

QnFfmpegVideoDecoder::QnFfmpegVideoDecoder(
    const DecoderConfig& config,
    nx::metrics::Storage* metrics,
    const QnConstCompressedVideoDataPtr& data)
    :
    m_context(0),
    m_codecId(data->compressionType),
    m_decodeMode(DecodeMode_Full),
    m_newDecodeMode(DecodeMode_NotDefined),
    m_lightModeFrameCounter(0),
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
    m_metrics(metrics),
    m_config(config)
{
    if (m_metrics)
        m_metrics->decoders()++;
    setMultiThreadDecoding(m_mtDecodingPolicy == MultiThreadDecodePolicy::enabled);
    openDecoder(data);
}

QnFfmpegVideoDecoder::~QnFfmpegVideoDecoder(void)
{
    avcodec_free_context(&m_context);
    if (m_metrics)
        m_metrics->decoders()--;
}

AVCodec* QnFfmpegVideoDecoder::findCodec(AVCodecID codecId)
{
    AVCodec* codec = 0;

    if (codecId != AV_CODEC_ID_NONE)
        codec = avcodec_find_decoder(codecId);

    return codec;
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
                const auto nalType = NALUnit::decodeType(*curNal);
                if (nalType >= nuSliceNonIDR && nalType <= nuSliceIDR) {
                    nx::utils::BitStreamReader bitReader;
                    try {
                        bitReader.setBuffer(curNal + 1, end);
                        int first_mb_in_slice = bitReader.getGolomb();
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
        // ignoring deblocking filter type 1 for better performance between H264 slices.
        m_context->flags2 |= AV_CODEC_FLAG2_FAST;
    }
    m_checkH264ResolutionChange =
        m_context->thread_count > 1 &&
        m_context->codec_id == AV_CODEC_ID_H264 &&
        (!m_context->extradata_size || m_context->extradata[0] == 0);

    if (m_config.forceGrayscaleDecoding)
        m_context->flags |= AV_CODEC_FLAG_GRAY;
}

bool QnFfmpegVideoDecoder::openDecoder(const QnConstCompressedVideoDataPtr& data)
{
    m_codec = findCodec(data->compressionType);
    m_context = avcodec_alloc_context3(m_codec);

    if (data->context)
    {
        data->context->toAvCodecContext(m_context);
        // TODO make correct check on changing decoder settings
        if (m_context->width > 8 && m_context->height > 8 && m_currentWidth == -1)
        {
            m_currentWidth = m_context->width;
            m_currentHeight = m_context->height;
        }
    }

    m_frameTypeExtractor = std::make_unique<FrameTypeExtractor>(
        CodecParametersConstPtr(new CodecParameters(m_context)));

    determineOptimalThreadType(data);

    int status = avcodec_open2(m_context, m_codec, NULL);
    if (status < 0)
    {
        NX_ERROR(this, "Failed to open ffmpeg video decoder, error: %1",
            QnFfmpegHelper::avErrorToString(status));
        return false;
    }
    // keep frame unless we call 'av_frame_unref'
    m_context->refcounted_frames = 1;
    return true;
}

bool QnFfmpegVideoDecoder::resetDecoder(const QnConstCompressedVideoDataPtr& data)
{
    if (!data || !(data->flags & AV_PKT_FLAG_KEY))
    {
        m_needRecreate = true;
        return true; // can't reset right now
    }
    avcodec_free_context(&m_context);
    m_spsFound = false;
    return openDecoder(data);
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
    if (m_lastDecodeResult < 0)
    {
        NX_DEBUG(this, "Ffmpeg decoder error: %1",
            QnFfmpegHelper::avErrorToString(m_lastDecodeResult));
    }
    if (m_lastDecodeResult > 0 && m_metrics)
        m_metrics->decodedPixels() += picture->width * picture->height;

    return m_lastDecodeResult;
}

// The input buffer must be AV_INPUT_BUFFER_PADDING_SIZE larger than the actual bytes read because
// some optimized bitstream readers read 32 or 64 bits at once and could read over the end. The end
// of the input buffer buf should be set to 0 to ensure that no overreading happens for damaged MPEG
// streams.
bool QnFfmpegVideoDecoder::decode(
    const QnConstCompressedVideoDataPtr& data, CLVideoDecoderOutputPtr* const outFramePtr)
{
    bool isImage = false;
    if (data)
    {
        isImage = data->flags.testFlag(QnAbstractMediaData::MediaFlags_StillImage)
            || isImageCanBeDecodedViaQt(data->compressionType);
    }

    if (data && m_codecId != data->compressionType)
    {
        if (m_codecId != AV_CODEC_ID_NONE && data->context)
        {
            if (!data->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
            {
                NX_DEBUG(this,
                    "Decoding should start from key frame, skip. codec: %1, timestamp: %2",
                    data->compressionType, data->timestamp);
                return false; //< Can't switch decoder to new codec right now.
            }
            resetDecoder(data);
        }
        m_codecId = data->compressionType;
    }

    CLVideoDecoderOutput* const outFrame = outFramePtr->data();
    int got_picture = 0;
    outFrame->clean();

    if (data)
    {
        m_lastFlags = data->flags;
        m_lastChannelNumber = data->channelNumber;

        if (m_codec == 0 && !isImage)
        {
            NX_WARNING(this, "Decoder not found, codec: %1", data->compressionType);
            return false;
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

        if (m_needRecreate && (data->flags & AV_PKT_FLAG_KEY))
        {
            m_needRecreate = false;
            resetDecoder(data);
        }

        QnFfmpegAvPacket avpkt((unsigned char*) data->data(), (int) data->dataSize());
        avpkt.dts = data->timestamp;
        avpkt.pts = AV_NOPTS_VALUE;
        // Tip for CorePNG to decode as normal PNG by default.
        avpkt.flags = AV_PKT_FLAG_KEY;

        // from avcodec_decode_video2() docs:
        // 1) The input buffer must be AV_INPUT_BUFFER_PADDING_SIZE larger than
        // the actual read bytes because some optimized bitstream readers read 32 or 64
        // bits at once and could read over the end.
        // 2) The end of the input buffer buf should be set to 0 to ensure that
        // no overreading happens for damaged MPEG streams.

        // 1 is already guaranteed by nx::utils::ByteArray, let's apply 2 here...
        if (avpkt.data)
            memset(avpkt.data + avpkt.size, 0, AV_INPUT_BUFFER_PADDING_SIZE);

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
            // It is slower but some cameras do not put SPS the beginning of a data.
            while (curPtr < dataEnd - 2)
            {
                const int nalLen = curPtr[2] == 0x01 ? 3 : 4;
                if (curPtr + nalLen >= dataEnd)
                    break;

                const auto nalUnitType = NALUnit::decodeType(curPtr[nalLen]);
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
            {
                NX_DEBUG(this, "SPS not found, codec: %1", data->compressionType);
                return false; // no sps has found yet. skip frame
            }
        }
        else if (data->context)
        {
            if (data->context->getWidth() != 0 && data->context->getHeight() != 0)
                processNewResolutionIfChanged(data, data->context->getWidth(), data->context->getHeight());
        }

        // -------------------------
        if(m_context->codec)
        {
            decodeVideo(m_context, outFrame, &got_picture, &avpkt);
            for(int i = 0; i < 2 && !got_picture && (data->flags & QnAbstractMediaData::MediaFlags_DecodeTwice); ++i)
                decodeVideo(m_context, outFrame, &got_picture, &avpkt);
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
                        const double fps = 1'000'000.0 / frameDistance;
                        qint64 megapixels = m_context->width * m_context->height * fps;
                        static const qint64 kThreshold = 1920*2 * 1080*2 * 20; //< 4k, 20fps
                        if (megapixels >= kThreshold)
                            setMultiThreadDecoding(true); // high fps and high resolution
                    }
                }
            }
            if (m_prevTimestamp != AV_NOPTS_VALUE)
                m_prevFrameDuration = data->timestamp - m_prevTimestamp;
            m_prevTimestamp = data->timestamp;

        }

        // sometimes ffmpeg can't decode image files. Try to decode in QT
        if (!got_picture && isImage)
        {
            QImage tmpQtImg;
            tmpQtImg.loadFromData(avpkt.data, avpkt.size);
            if (tmpQtImg.width() > 0 && tmpQtImg.height() > 0) {
                // TODO: #dklychkov Maybe set correct ffmpeg format if possible instead of conversion
                if (tmpQtImg.format() != QImage::Format_RGBA8888)
                    tmpQtImg = tmpQtImg.convertToFormat(QImage::Format_RGBA8888);

                got_picture = 1;
                CLVideoDecoderOutput tmpFrame;
                tmpFrame.clean();
                tmpFrame.setUseExternalData(true);
                tmpFrame.format = AV_PIX_FMT_RGBA;
                tmpFrame.data[0] = (quint8*) tmpQtImg.constBits();
                tmpFrame.linesize[0] = tmpQtImg.bytesPerLine();
                m_context->width = tmpFrame.width = tmpQtImg.width();
                m_context->height = tmpFrame.height = tmpQtImg.height();
                tmpFrame.pkt_dts = data->timestamp;
                outFrame->copyFrom(&tmpFrame);
                return true;
            }
        }
    }
    else
    {
        QnFfmpegAvPacket avpkt;
        decodeVideo(m_context, outFrame, &got_picture, &avpkt); // flush

        // In case of b-frames stream ffmpeg gives last flushed frames with AV_NOPTS_VALUE, so use
        // m_prevFrameDuration to get frame pts
        if (outFrame->pkt_dts == AV_NOPTS_VALUE)
        {
            outFrame->pkt_dts = m_prevTimestamp + m_prevFrameDuration;
            m_prevTimestamp = outFrame->pkt_dts;
        }
    }

    if (m_config.forceRgbaFormat && got_picture && outFrame->format != AV_PIX_FMT_RGBA)
    {
        CLVideoDecoderOutput tmpFrame;
        tmpFrame.reallocate(outFrame->width, outFrame->height, AV_PIX_FMT_RGBA);
        if (data)
            tmpFrame.pkt_dts = data->timestamp;
        if (outFrame->convertTo(&tmpFrame))
        {
            outFrame->copyFrom(&tmpFrame);
            outFrame->fillRightEdge();
            outFrame->sample_aspect_ratio = getSampleAspectRatio();
            outFrame->flags = m_lastFlags;
            outFrame->channel = m_lastChannelNumber;
            return m_context->pix_fmt != AV_PIX_FMT_NONE;
        }
    }

    if (got_picture)
    {
        outFrame->format = CLVideoDecoderOutput::fixDeprecatedPixelFormat(m_context->pix_fmt);
        outFrame->fillRightEdge();
        outFrame->sample_aspect_ratio = getSampleAspectRatio();
        outFrame->flags = m_lastFlags;
        outFrame->channel = m_lastChannelNumber;
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

bool QnFfmpegVideoDecoder::hardwareDecoder() const
{
    return false;
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

AVCodecContext* QnFfmpegVideoDecoder::getContext() const
{
    return m_context;
}

void QnFfmpegVideoDecoder::setMultiThreadDecodePolicy(MultiThreadDecodePolicy value)
{
    if (m_mtDecodingPolicy != value)
    {
        NX_VERBOSE(this, nx::format("Use multithread decoding policy %1").arg((int) value));
        m_mtDecodingPolicy = value;
        setMultiThreadDecoding(m_mtDecodingPolicy == MultiThreadDecodePolicy::enabled);
    }
}
