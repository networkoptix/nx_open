// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_video_transcoder.h"

#include <QtCore/QThread>

extern "C" {
#include <libavutil/hwcontext.h>
} // extern "C"

#include <decoders/video/ffmpeg_video_decoder.h>
#include <nx/media/codec_parameters.h>
#include <nx/media/config.h>
#include <nx/media/ffmpeg/av_options.h>
#include <nx/media/ffmpeg/av_packet.h>
#include <nx/media/ffmpeg/ffmpeg_utils.h>
#include <nx/media/ffmpeg/hw_video_decoder.h>
#include <nx/media/utils.h>
#include <nx/media/video_data_packet.h>
#include <nx/metric/metrics_storage.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/math.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/common/nx_vms_common_ini.h>
#include <transcoding/transcoding_utils.h>
#include <transcoding/video_processor.h>
#include <utils/common/util.h>

#include "filters/abstract_image_filter.h"
#include "filters/crop_image_filter.h"

namespace {
const static qint64 OPTIMIZATION_BEGIN_FRAME = 10;
const static qint64 OPTIMIZATION_MOVING_AVERAGE_RATE = 90;
static const int kMaxDroppedFrames = 5;
constexpr AVRational kMicroseconds{1, 1'000'000};

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
    m_metrics(metrics),
    m_metadataObjectsCache(MetadataType::ObjectDetection, nx::analytics::kMetadataCacheDuration),
    m_metadataMotionCache(MetadataType::Motion, nx::analytics::kMetadataCacheDuration)
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
    m_encoder.close();
    m_lastFlushedDecoder = 0;
    m_videoDecoders.clear();
    m_processor.reset();
    av_buffer_unref(&m_hwDevice);
}

bool QnFfmpegVideoTranscoder::prepareFilters(
    AVCodecID dstCodec, const QnConstCompressedVideoDataPtr& video)
{
    if (!m_filters)
    {
        // This need to align the target resolution(in prepareDownscaleFilter function).
        // #TODO make it without filters or more explicitly.
        m_filters = std::make_unique<nx::core::transcoding::FilterChain>(
            nx::core::transcoding::Settings());
    }
    if (m_filters->isReady())
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

    m_filters->prepare(m_sourceResolution, nx::transcoding::maxResolution(dstCodec));
    m_targetResolution = m_filters->apply(m_sourceResolution);
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

    if (nx::vms::common::ini().enableHwTranscoding)
    {
        if (const int status =
                av_hwdevice_ctx_create(&m_hwDevice, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0);
            status < 0)
        {
            NX_WARNING(this,
                "Unable to open a VAAPI device: %1, transcoding in software",
                nx::media::ffmpeg::avErrorToString(status));
        }
    }

    m_processor = std::make_unique<nx::transcoding::VideoProcessor>(
        *m_filters, m_sourceResolution, m_targetResolution, m_hwDevice);
    if (!m_encoder.open(m_config.targetCodecId,
            m_targetResolution,
            m_hwDevice,
            [this](AVCodecContext* context, nx::media::ffmpeg::AvOptions& options)
            { configureEncoder(context, options); }))
    {
        close();
        return false;
    }

    if (!m_codecParamaters)
        m_codecParamaters.reset(new CodecParameters(m_encoder.context()));

    m_encodeTimer.start();
    return true;
}

void QnFfmpegVideoTranscoder::configureEncoder(
    AVCodecContext* context, nx::media::ffmpeg::AvOptions& options) const
{
    context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    context->bit_rate = m_config.bitrate != -1
        ? m_config.bitrate
        : suggestBitrate(
              m_config.targetCodecId, m_targetResolution, m_config.quality, context->codec->name);
    context->gop_size = 32;
    context->time_base.num = 1;
    context->time_base.den = m_config.fixedFrameRate ? m_config.fixedFrameRate : 60;
    context->sample_aspect_ratio.den = context->sample_aspect_ratio.num = 1;
    if (m_config.useMultiThreadEncode && m_config.targetCodecId != AV_CODEC_ID_MJPEG
        && !context->hw_frames_ctx)
    {
        context->thread_count = qMin(2, QThread::idealThreadCount());
    }

    for (auto it = m_config.params.begin(); it != m_config.params.end(); ++it)
    {
        if( it.key() == QnCodecParams::global_quality )
        {
            //100 - 1;
            //0 - FF_LAMBDA_MAX;
            context->global_quality = INT_MAX / 50 * (it.value().toInt() - 50);
        }
        else if( it.key() == QnCodecParams::qscale )
        {
            context->flags |= AV_CODEC_FLAG_QSCALE;
            context->global_quality = FF_QP2LAMBDA * it.value().toInt();
        }
        else if( it.key() == QnCodecParams::qmin )
        {
            context->qmin = it.value().toInt();
        }
        else if( it.key() == QnCodecParams::qmax )
        {
            context->qmax = it.value().toInt();
        }
        else if (it.key() == QnCodecParams::gop_size)
        {
            context->gop_size = it.value().toInt();
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
}

AVCodecParameters* QnFfmpegVideoTranscoder::getCodecParameters()
{
    if (m_codecParamaters)
        return m_codecParamaters->getAvCodecParameters();

    return nullptr;
}

int QnFfmpegVideoTranscoder::transcodePacket(const QnConstAbstractMediaDataPtr& media, QnAbstractMediaDataPtr* const result)
{
    NX_VERBOSE(this, "Transcode video data: %1, target codec: %2", media, m_config.targetCodecId);

    m_encodeTimer.restart();
    if (result)
        result->reset();

    const auto video = std::dynamic_pointer_cast<const QnCompressedVideoData>(media);
    int ret = transcodePacketImpl(video, result);
    // Callers stop flushing at the first empty result: go on while the decoders hold frames.
    while (!ret && !media && result && !*result && !m_videoDecoders.empty())
        ret = transcodePacketImpl(nullptr, result);
    if (ret)
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

std::pair<uint32_t, QnAbstractVideoDecoder*> QnFfmpegVideoTranscoder::getDecoder(
    const QnConstCompressedVideoDataPtr& video)
{
    if (video)
    {
        auto& decoder = m_videoDecoders[video->channelNumber];
        if (!decoder && m_hwDevice)
        {
            auto hwDecoder = std::make_unique<nx::media::ffmpeg::HwVideoDecoder>(
                AV_HWDEVICE_TYPE_VAAPI, m_metrics);
            hwDecoder->setHwDeviceContext(m_hwDevice);
            hwDecoder->setMultiThreadDecodePolicy(m_config.decoderConfig.mtDecodePolicy);
            decoder = std::move(hwDecoder);
        }
        else if (!decoder)
        {
            decoder =
                std::make_unique<QnFfmpegVideoDecoder>(m_config.decoderConfig, m_metrics, video);
        }
        return std::make_pair(video->channelNumber, decoder.get());
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
    if (!m_encoder.context() && video)
    {
        if (!open(video))
        {
            NX_WARNING(this, "Failed to open video transcoder");
            return false;
        }
    }

    const auto encoder = m_encoder.context();
    if (!encoder)
    {
        NX_DEBUG(this, "Invalid state, empty encoder context");
        return -3; //< encode error
    }

    const auto [channelNumber, decoder] = getDecoder(video);
    if (!decoder)
    {
        // Every decoder is drained: drain the hardware encoder, it holds packets back.
        return !video && m_encoder.isHardware() && result ? encodeFrame({}, result) : 0;
    }

    if (video)
        m_deviceId = video->deviceId;

    if (result)
        *result = QnCompressedVideoDataPtr();

    CLVideoDecoderOutputPtr decodedFrame(new CLVideoDecoderOutput());
    if (!decoder->decode(video, &decodedFrame))
    {
        if (video)
            return 0;
        m_videoDecoders.erase(channelNumber); //< Drained, flush the next channel.
        return transcodePacketImpl(nullptr, result);
    }

    // HwVideoDecoder passes timestamps as pts, its pkt_dts is that of the packet that triggered
    // the frame, out of order with B-frames and unset for the flushed ones.
    if (m_hwDevice)
        decodedFrame->pkt_dts = decodedFrame->best_effort_timestamp;
    if (decodedFrame->pkt_dts < m_config.startTimeUs)
        return 0; // Ignore frames before start time.

    if (video && video->flags.testFlag(QnAbstractMediaData::MediaFlags_Ignore))
        return 0; // do not transcode ignored frames

    decodedFrame->channel = channelNumber;
    decodedFrame->pts = decodedFrame->pkt_dts;

    using namespace std::chrono;
    const milliseconds timestamp =
        duration_cast<milliseconds>(microseconds(decodedFrame->pkt_dts));
    m_filters->setMetadata(m_metadataObjectsCache.metadata(timestamp, decodedFrame->channel),
        m_metadataMotionCache.metadata(timestamp, decodedFrame->channel));
    decodedFrame = m_processor->process(decodedFrame);

    if (!decodedFrame)
    {
        NX_ERROR(this, "Failed to process filter chain for video frame");
        return 0;
    }

    if (m_config.fixedFrameRate)
    {
        m_frameNumToPts[encoder->frame_num] = decodedFrame->pts;
        decodedFrame->pts = encoder->frame_num;
    }
    else
    {
        decodedFrame->pts = av_rescale_q(decodedFrame->pts, kMicroseconds, encoder->time_base);
    }

    // FFmpeg encoder failed when PTS <= last PTS, (at least mjpeg encoder)
    if (m_lastEncodedPts && decodedFrame->pts <= m_lastEncodedPts)
        decodedFrame->pts = m_lastEncodedPts.value() + 1;

    m_lastEncodedPts = decodedFrame->pts;

    if( !result )
        return 0;

    // Improve performance by omitting frames to encode in case if encoding goes way too slow..
    // TODO: #muskov make mediaserver's config option and HTTP query option to disable feature
    if (m_config.useRealTimeOptimization && encoder->frame_num > OPTIMIZATION_BEGIN_FRAME)
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
    return encodeFrame(decodedFrame, result);
}

int QnFfmpegVideoTranscoder::encodeFrame(
    const CLVideoDecoderOutputPtr& frame, QnAbstractMediaDataPtr* const result)
{
    nx::media::ffmpeg::AvPacket avPacket;
    auto packet = avPacket.get();
    int got_packet = 0;
    int encodeResult = m_encoder.encode(frame, packet, &got_packet);
    if (encodeResult < 0)
    {
        NX_WARNING(this, "Failed to encode video, error: %1",
            nx::media::ffmpeg::avErrorToString(encodeResult));
        return encodeResult;
    }
    if (m_metrics && frame)
        m_metrics->encodedPixels() += frame->width * frame->height;
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
        resultVideoData->timestamp =
            av_rescale_q(packet->pts, m_encoder.context()->time_base, kMicroseconds);
    }

    if (packet->flags & AV_PKT_FLAG_KEY)
        resultVideoData->flags |= QnAbstractMediaData::MediaFlags_AVKey;

    resultVideoData->m_data.write((const char*) packet->data, packet->size); // todo: remove data copy here!
    resultVideoData->compressionType = updateCodec(m_config.targetCodecId);

    if (!m_codecParamaters)
        m_codecParamaters.reset(new CodecParameters(m_encoder.context()));
    resultVideoData->context = m_codecParamaters;
    resultVideoData->width = m_encoder.context()->width;
    resultVideoData->height = m_encoder.context()->height;
    resultVideoData->deviceId = m_deviceId;
    *result = QnCompressedVideoDataPtr(resultVideoData);

    m_droppedFrames = 0;
    return 0;
}

void QnFfmpegVideoTranscoder::processMetadata(const QnConstAbstractCompressedMetadataPtr& metadata)
{
    if (metadata->metadataType == MetadataType::ObjectDetection)
    {
        m_metadataObjectsCache.processMetadata(metadata);
    }

    if (metadata->metadataType == MetadataType::Motion)
    {
        m_metadataMotionCache.processMetadata(metadata);
    }
}

AVCodecContext* QnFfmpegVideoTranscoder::getCodecContext()
{
    return m_encoder.context();
}

QSize QnFfmpegVideoTranscoder::getOutputResolution() const
{
    return m_targetResolution;
}

void QnFfmpegVideoTranscoder::setFilterChain(nx::core::transcoding::FilterChainPtr filters)
{
    m_filters = std::move(filters);
}
