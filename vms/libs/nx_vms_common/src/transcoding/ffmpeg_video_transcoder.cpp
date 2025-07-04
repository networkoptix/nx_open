// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_video_transcoder.h"

#include <QtCore/QThread>

#include <common/common_module.h>
#include <decoders/video/ffmpeg_video_decoder.h>
#include <nx/media/codec_parameters.h>
#include <nx/media/config.h>
#include <nx/media/ffmpeg/av_options.h>
#include <nx/media/ffmpeg/av_packet.h>
#include <nx/media/ffmpeg/ffmpeg_utils.h>
#include <nx/media/ffmpeg/old_api.h>
#include <nx/media/utils.h>
#include <nx/media/video_data_packet.h>
#include <nx/metric/metrics_storage.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/math.h>
#include <nx/utils/scope_guard.h>
#include <transcoding/transcoding_utils.h>
#include <utils/common/util.h>

#include "filters/abstract_image_filter.h"
#include "filters/crop_image_filter.h"

namespace {
const static qint64 OPTIMIZATION_BEGIN_FRAME = 10;
const static qint64 OPTIMIZATION_MOVING_AVERAGE_RATE = 90;
static const int kMaxDroppedFrames = 5;
}

static const nx::log::Tag kLogTag(QString("Transcoding"));

AVCodecID findVideoEncoder(const QString& codecName)
{
    AVCodecID codecId = nx::transcoding::findEncoderCodecId(codecName);
    if (codecId == AV_CODEC_ID_NONE)
    {
        NX_WARNING(kLogTag, "Configured codec: %1 not found, h263p will used", codecName);
        return AV_CODEC_ID_H263P;
    }
    return codecId;
}

namespace {

    static qint64& movigAverage(qint64& accumulator, qint64 value)
    {
        static_assert(OPTIMIZATION_MOVING_AVERAGE_RATE < 100, "Moving average K should be below 100%");
        static const auto movingAvg = static_cast<double>(OPTIMIZATION_MOVING_AVERAGE_RATE) / 100.0;

        if (accumulator == 0)
            return accumulator = value;

        return accumulator = accumulator * movingAvg + value * (1.0 - movingAvg);
    }

    AVCodecID updateCodec(AVCodecID codec)
    {
        if (codec == AV_CODEC_ID_H263P)
            return AV_CODEC_ID_H263;
        else
            return codec;
    }
}

QnFfmpegVideoTranscoder::QnFfmpegVideoTranscoder(
    const Config& config, nx::metric::Storage* metrics)
    :
    m_config(config),
    m_metrics(metrics)
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
    {
        m_lastSrcWidth[i] = -1;
        m_lastSrcHeight[i] = -1;
    }
    if (m_metrics)
        m_metrics->transcoders()++;
}

QnFfmpegVideoTranscoder::~QnFfmpegVideoTranscoder()
{
    close();
    if (m_metrics)
        m_metrics->transcoders()--;
}

void QnFfmpegVideoTranscoder::close()
{
    if (m_encoderCtx)
        avcodec_free_context(&m_encoderCtx);
    m_lastFlushedDecoder = 0;
    m_videoDecoders.clear();
}

bool QnFfmpegVideoTranscoder::prepareFilters(
    AVCodecID dstCodec, const QnConstCompressedVideoDataPtr& video)
{
    if (m_filters.isReady())
        return true;

    m_sourceResolution = m_config.sourceResolution;
    if (!m_sourceResolution.isValid())
    {
        NX_DEBUG(this, "Failed to get max resolution from resource, try to get it from data");
        m_sourceResolution = nx::media::getFrameSize(video.get());
    }

    if (!m_sourceResolution.isValid())
    {
        NX_WARNING(this, "Invalid video data, failed to get resolution");
        return false;
    }

    m_outputResolutionLimit = m_config.outputResolutionLimit;
    if (m_outputResolutionLimit.isValid())
    {
        m_outputResolutionLimit = nx::transcoding::normalizeResolution(
            m_outputResolutionLimit, m_sourceResolution);
        if (m_outputResolutionLimit.isEmpty())
        {
            NX_WARNING(this, "Invalid resolution specified source %1, target %2",
                m_sourceResolution, m_outputResolutionLimit);
            return false;
        }
        m_outputResolutionLimit = alignSizeRound(m_outputResolutionLimit, 16, 4);

        // Use user resolution if it less as a source before filters.
        if (m_outputResolutionLimit.height() < m_sourceResolution.height())
            m_sourceResolution = m_outputResolutionLimit;
    }
    m_filters.prepare(m_sourceResolution, nx::transcoding::maxResolution(dstCodec));
    m_targetResolution = m_filters.apply(m_sourceResolution);
    NX_DEBUG(this, "Prepare transcoding, output resolution: %1, source resolution: %2",
        m_targetResolution, m_sourceResolution);
    return true;
}

bool QnFfmpegVideoTranscoder::open(const QnConstCompressedVideoDataPtr& video)
{
    close();

    if (!video)
        return false;

    if (!prepareFilters(m_config.targetCodecId, video))
        return false;

    const AVCodec* avCodec = avcodec_find_encoder(m_config.targetCodecId);
    if (avCodec == 0)
    {
        NX_WARNING(this, "Could not find encoder for codec %1.", m_config.targetCodecId);
        return false;
    }

    m_encoderCtx = avcodec_alloc_context3(avCodec);
    m_encoderCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    m_encoderCtx->codec_id = m_config.targetCodecId;
    m_encoderCtx->width = m_targetResolution.width();
    m_encoderCtx->height = m_targetResolution.height();
    m_encoderCtx->pix_fmt = m_config.targetCodecId == AV_CODEC_ID_MJPEG ? AV_PIX_FMT_YUVJ420P : AV_PIX_FMT_YUV420P;
    m_encoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    m_bitrate = m_config.bitrate;
    if (m_bitrate == -1)
    {
        m_bitrate = suggestBitrate(
            m_config.targetCodecId,
            QSize(m_encoderCtx->width,m_encoderCtx->height),
            m_config.quality, avCodec->name);
    }
    m_encoderCtx->bit_rate = m_bitrate;
    m_encoderCtx->gop_size = 32;
    m_encoderCtx->time_base.num = 1;
    m_encoderCtx->time_base.den = m_config.fixedFrameRate ? m_config.fixedFrameRate : 60;
    m_encoderCtx->sample_aspect_ratio.den = m_encoderCtx->sample_aspect_ratio.num = 1;
    if (m_config.useMultiThreadEncode && m_config.targetCodecId != AV_CODEC_ID_MJPEG)
        m_encoderCtx->thread_count = qMin(2, QThread::idealThreadCount());

    nx::media::ffmpeg::AvOptions options;
    for (auto it = m_config.params.begin(); it != m_config.params.end(); ++it)
    {
        if( it.key() == QnCodecParams::global_quality )
        {
            //100 - 1;
            //0 - FF_LAMBDA_MAX;
            m_encoderCtx->global_quality = INT_MAX / 50 * (it.value().toInt() - 50);
        }
        else if( it.key() == QnCodecParams::qscale )
        {
            m_encoderCtx->flags |= AV_CODEC_FLAG_QSCALE;
            m_encoderCtx->global_quality = FF_QP2LAMBDA * it.value().toInt();
        }
        else if( it.key() == QnCodecParams::qmin )
        {
            m_encoderCtx->qmin = it.value().toInt();
        }
        else if( it.key() == QnCodecParams::qmax )
        {
            m_encoderCtx->qmax = it.value().toInt();
        }
        else if (it.key() == QnCodecParams::gop_size)
        {
            m_encoderCtx->gop_size = it.value().toInt();
        }
        else
        {
            options.set(it.key().constData(), it.value().toByteArray().constData(), 0);
        }
    }

    if (m_config.rtpContatiner && m_config.targetCodecId == AV_CODEC_ID_MJPEG)
    {
        options.set("force_duplicated_matrix", 1, 0);
        options.set("huffman", "default");
    }

    if (avcodec_open2(m_encoderCtx, avCodec, options) < 0)
    {
        NX_WARNING(this, "Could not initialize video encoder.");
        close();
        return false;
    }

    if (!m_codecParamaters)
        m_codecParamaters.reset(new CodecParameters(m_encoderCtx));

    m_encodeTimer.start();
    return true;
}

AVCodecParameters* QnFfmpegVideoTranscoder::getCodecParameters()
{
    if (m_codecParamaters)
        return m_codecParamaters->getAvCodecParameters();

    return nullptr;
}

int QnFfmpegVideoTranscoder::transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result)
{
    m_encodeTimer.restart();

    if( result )
        result->reset();

    const auto video = std::dynamic_pointer_cast<const QnCompressedVideoData>(media);
    if (auto ret = transcodePacketImpl(video, result))
        return ret;

    if (video)
    {
        if (m_lastEncodedTime != qint64(AV_NOPTS_VALUE))
            movigAverage(m_averageVideoTimePerFrame, video->timestamp - m_lastEncodedTime);
        m_lastEncodedTime = video->timestamp;
    }

    movigAverage(m_averageCodingTimePerFrame, m_encodeTimer.elapsed() * 1000 /*mks*/);
    return 0;
}

std::pair<uint32_t, QnFfmpegVideoDecoder*> QnFfmpegVideoTranscoder::getDecoder(
        const QnConstCompressedVideoDataPtr& video)
{
    QnFfmpegVideoDecoder* decoder = nullptr;
    if (video)
    {
        auto decoderIter = m_videoDecoders.find(video->channelNumber);
        if (decoderIter != m_videoDecoders.end())
            return std::make_pair(decoderIter->first, decoderIter->second.get());

        decoder = new QnFfmpegVideoDecoder(m_config.decoderConfig, m_metrics, video);
        m_videoDecoders[video->channelNumber].reset(decoder);
        return std::make_pair(video->channelNumber, decoder);
    }
    else
    {
        // Flush decoders.
        if (m_videoDecoders.empty())
            return std::make_pair(0, nullptr);
        auto decoderIter = m_videoDecoders.begin();
        m_lastFlushedDecoder = m_lastFlushedDecoder % m_videoDecoders.size();
        decoderIter = std::next(decoderIter, m_lastFlushedDecoder);
        ++m_lastFlushedDecoder;
        return std::make_pair(decoderIter->first, decoderIter->second.get());
    }
}

int QnFfmpegVideoTranscoder::transcodePacketImpl(const QnConstCompressedVideoDataPtr& video, QnAbstractMediaDataPtr* const result)
{
    if (!m_encoderCtx && video)
    {
        if (!open(video))
        {
            NX_WARNING(this, "Failed to open video transcoder");
            return false;
        }
    }

    if (!m_encoderCtx)
    {
        NX_DEBUG(this, "Invalid state, empty encoder context");
        return -3; //< encode error
    }

    const auto [channelNumber, decoder] = getDecoder(video);
    if (!decoder)
        return 0;

    if (result)
        *result = QnCompressedVideoDataPtr();

    CLVideoDecoderOutputPtr decodedFrame(new CLVideoDecoderOutput());
    if (!decoder->decode(video, &decodedFrame))
        return 0; // ignore decode error

    if (decodedFrame->pkt_dts < m_config.startTimeUs)
        return 0; // Ignore frames before start time.

    if (video && video->flags.testFlag(QnAbstractMediaData::MediaFlags_Ignore))
        return 0; // do not transcode ignored frames

    decodedFrame->channel = channelNumber;
    decodedFrame->pts = decodedFrame->pkt_dts;

    // Second stream must be upscaled to the first stream resolution before the filters apply.
    if (decodedFrame->size() != m_sourceResolution || decodedFrame->format != AV_PIX_FMT_YUV420P)
    {
        decodedFrame = decodedFrame->scaled(m_sourceResolution, AV_PIX_FMT_YUV420P);
        if (!decodedFrame)
        {
            NX_ERROR(this, "Failed to scale video frame to %1", m_sourceResolution);
            return 0;
        }
    }

    decodedFrame = m_filters.apply(decodedFrame, /*metadata*/ {});
    if (!decodedFrame)
    {
        NX_ERROR(this, "Failed to process filter chain for video frame");
        return 0;
    }

    static AVRational r = { 1, 1'000'000 };
    if (m_config.fixedFrameRate)
    {
        m_frameNumToPts[m_encoderCtx->frame_num] = decodedFrame->pts;
        decodedFrame->pts = m_encoderCtx->frame_num;
    }
    else
    {
        decodedFrame->pts = av_rescale_q(decodedFrame->pts, r, m_encoderCtx->time_base);
    }

    // FFmpeg encoder failed when PTS <= last PTS, (at least mjpeg encoder)
    if (m_lastEncodedPts && decodedFrame->pts <= m_lastEncodedPts)
        decodedFrame->pts = m_lastEncodedPts.value() + 1;

    m_lastEncodedPts = decodedFrame->pts;

    if( !result )
        return 0;

    // Improve performance by omitting frames to encode in case if encoding goes way too slow..
    // TODO: #muskov make mediaserver's config option and HTTP query option to disable feature
    if (m_config.useRealTimeOptimization && m_encoderCtx->frame_num > OPTIMIZATION_BEGIN_FRAME)
    {
        // the more frames are dropped the lower limit is gonna be set
        // (x + (x >> k)) is faster and more precise then (x * (1 + 0.5^k))
        const auto& codingTime = m_averageCodingTimePerFrame;
        if (codingTime + (codingTime >> m_droppedFrames) > m_averageVideoTimePerFrame)
        {
            if (++m_droppedFrames < kMaxDroppedFrames)
                return 0;
        }
    }
    decodedFrame->pict_type = AV_PICTURE_TYPE_NONE;

    nx::media::ffmpeg::AvPacket avPacket;
    auto packet = avPacket.get();
    int got_packet = 0;
    int encodeResult = nx::media::ffmpeg::old_api::encode(
        m_encoderCtx, packet, decodedFrame.data(), &got_packet);
    if (encodeResult < 0)
    {
        NX_WARNING(this, "Failed to encode video, error: %1",
            nx::media::ffmpeg::avErrorToString(encodeResult));
        return encodeResult;
    }
    if (m_metrics)
        m_metrics->encodedPixels() += decodedFrame->width * decodedFrame->height;
    if (!got_packet)
        return 0;

    auto resultVideoData = new QnWritableCompressedVideoData(packet->size);

    if (m_config.fixedFrameRate)
    {
        auto itr = m_frameNumToPts.find(packet->pts);
        if (itr != m_frameNumToPts.end())
        {
            resultVideoData->timestamp = itr->second;
            m_frameNumToPts.erase(itr);
        }
    }
    else
    {
        resultVideoData->timestamp = av_rescale_q(packet->pts, m_encoderCtx->time_base, r);
    }

    if (packet->flags & AV_PKT_FLAG_KEY)
        resultVideoData->flags |= QnAbstractMediaData::MediaFlags_AVKey;

    resultVideoData->m_data.write((const char*) packet->data, packet->size); // todo: remove data copy here!
    resultVideoData->compressionType = updateCodec(m_config.targetCodecId);

    if (!m_codecParamaters)
        m_codecParamaters.reset(new CodecParameters(m_encoderCtx));
    resultVideoData->context = m_codecParamaters;
    resultVideoData->width = m_encoderCtx->width;
    resultVideoData->height = m_encoderCtx->height;
    *result = QnCompressedVideoDataPtr(resultVideoData);

    m_droppedFrames = 0;
    return 0;
}

AVCodecContext* QnFfmpegVideoTranscoder::getCodecContext()
{
    return m_encoderCtx;
}

QSize QnFfmpegVideoTranscoder::getOutputResolution() const
{
    return m_targetResolution;
}

void QnFfmpegVideoTranscoder::setFilterChain(const nx::core::transcoding::FilterChain& filters)
{
    m_filters = filters;
}
