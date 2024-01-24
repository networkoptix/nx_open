// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "transcoder.h"

#include <QtCore/QThread>

#include <core/resource/media_resource.h>
#include <nx/media/config.h>
#include <nx/media/sse_helper.h>
#include <nx/metrics/metrics_storage.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/math.h>
#include <transcoding/ffmpeg_audio_transcoder.h>
#include <transcoding/ffmpeg_transcoder.h>
#include <transcoding/ffmpeg_video_transcoder.h>
#include <transcoding/filters/abstract_image_filter.h>
#include <transcoding/filters/filter_helper.h>

// ---------------------------- QnCodecTranscoder ------------------
QnCodecTranscoder::QnCodecTranscoder(AVCodecID codecId)
:
    m_bitrate(-1),
    m_quality(Qn::StreamQuality::normal)
{
    m_codecId = codecId;
}

void QnCodecTranscoder::setParams(const QnCodecParams::Value& params)
{
    m_params = params;
}

void QnCodecTranscoder::setBitrate(int value)
{
    m_bitrate = value;
}

int QnCodecTranscoder::getBitrate() const
{
    return m_bitrate;
}

/*
AVCodecContext* QnCodecTranscoder::getCodecContext()
{
    return 0;
}
*/

void QnCodecTranscoder::setQuality( Qn::StreamQuality quality )
{
    m_quality = quality;
}

QRect QnCodecTranscoder::roundRect(const QRect& srcRect)
{
    int left = qPower2Floor((unsigned) srcRect.left(), CL_MEDIA_ALIGNMENT);
    int top = qPower2Floor((unsigned) srcRect.top(), 2);

    int width = qPower2Ceil((unsigned) srcRect.width(), 16);
    int height = qPower2Ceil((unsigned) srcRect.height(), 2);

    return QRect(left, top, width, height);
}

QSize QnCodecTranscoder::roundSize(const QSize& size)
{
    int width = qPower2Ceil((unsigned) size.width(), 16);
    int height = qPower2Ceil((unsigned) size.height(), 4);

    return QSize(width, height);
}

// --------------------------- QnVideoTranscoder -----------------
QnVideoTranscoder::QnVideoTranscoder(AVCodecID codecId):
    QnCodecTranscoder(codecId)
{
}

// ---------------------- QnTranscoder -------------------------
QnTranscoder::QnTranscoder(const DecoderConfig& decoderConfig, nx::metrics::Storage* metrics)
    :
    m_decoderConfig(decoderConfig),
    m_videoCodec(AV_CODEC_ID_NONE),
    m_audioCodec(AV_CODEC_ID_NONE),
    m_videoStreamCopy(false),
    m_audioStreamCopy(false),
    m_internalBuffer(CL_MEDIA_ALIGNMENT, 1024*1024, AV_INPUT_BUFFER_PADDING_SIZE),
    m_initialized(false),
    m_initializedAudio(false),
    m_initializedVideo(false),
    m_metrics(metrics),
    m_eofCounter(0),
    m_packetizedMode(false),
    m_useRealTimeOptimization(false)
{
    QThread::currentThread()->setPriority(QThread::LowPriority);
}

QnTranscoder::~QnTranscoder()
{

}

int QnTranscoder::suggestBitrate(
    AVCodecID /*codec*/,
    QSize resolution,
    Qn::StreamQuality quality,
    const char* codecName)
{
    // I assume for a Qn::StreamQuality::highest quality 30 fps for 1080 we need 10 mbps
    // I assume for a Qn::StreamQuality::lowest quality 30 fps for 1080 we need 1 mbps

    if (resolution.width() == 0)
        resolution.setWidth(resolution.height() * 4 / 3);

    int hiEnd;
    switch (quality)
    {
        case Qn::StreamQuality::lowest:
            hiEnd = 1024;
            break;
        case Qn::StreamQuality::low:
            hiEnd = 1024 + 512;
            break;
        case Qn::StreamQuality::normal:
            hiEnd = 1024 * 2;
            break;
        case Qn::StreamQuality::high:
            hiEnd = 1024 * 3;
            break;
        case Qn::StreamQuality::rapidReview:
            hiEnd = 1024 * 10;
            break;
        case Qn::StreamQuality::highest:
        default:
            hiEnd = 1024 * 5;
            break;
    }

    float resolutionFactor = resolution.width()*resolution.height() / 1920.0 / 1080;
    resolutionFactor = pow(resolutionFactor, (float)0.63); // 256kbps for 320x240 for Mq quality

    float codecFactor = 1;
    if (codecName)
    {
        // Increase bitrate due to bad quality of libopenh264 coding.
        if (strcmp(codecName, "mpeg4") == 0)
            codecFactor = 1.2;
        else if (strcmp(codecName, "libopenh264") == 0)
            codecFactor = 4;
    }

    int result = hiEnd * resolutionFactor * codecFactor;
    return qMax(128, result) * 1024;
}

QnCodecParams::Value QnTranscoder::suggestMediaStreamParams(
    AVCodecID codec,
    Qn::StreamQuality quality)
{
    QnCodecParams::Value params;

    switch (codec)
    {
        case AV_CODEC_ID_MJPEG:
        {
            int qVal = 1;
            switch( quality )
            {
                case Qn::StreamQuality::lowest:
                    qVal = 8;
                    break;
                case Qn::StreamQuality::low:
                    qVal = 4;
                    break;
                case Qn::StreamQuality::normal:
                    qVal = 3;
                    break;
                case Qn::StreamQuality::high:
                    qVal = 2;
                    break;
                case Qn::StreamQuality::highest:
                case Qn::StreamQuality::rapidReview:
                    qVal = 1;
                    break;
                default:
                    break;
            }

            params.insert( QnCodecParams::qmin, qVal );
            params.insert( QnCodecParams::qmax, 20);
        }
        break;
        case AV_CODEC_ID_VP8:
        {
            int cpuUsed = 0;
            int staticThreshold = 0;

            switch (quality)
            {
                case Qn::StreamQuality::lowest:
                    cpuUsed = 5;
                    break;
                case Qn::StreamQuality::low:
                    cpuUsed = 4;
                    break;
                case Qn::StreamQuality::normal:
                    cpuUsed = 3;
                    break;
                case Qn::StreamQuality::high:
                    cpuUsed = 1;
                    break;
                case Qn::StreamQuality::highest:
                case Qn::StreamQuality::rapidReview:
                    cpuUsed = 0;
                    break;
                default:
                    break;
            }

            if (quality <= Qn::StreamQuality::normal)
            {
                // [0..3] Bigger numbers mean less possibilities for encoder.
                params.insert("profile", 1);
                staticThreshold = 1000;
            }

            // Options are taken from https://www.webmproject.org/docs/encoder-parameters/

            params.insert("good", QString());
            params.insert("cpu-used", cpuUsed);
            params.insert("kf-min-dist", 0);
            params.insert("kf-max-dist", 360);
            params.insert("token-parts", 2);
            params.insert("static-thresh", staticThreshold);
            params.insert("min-q", 0);
            params.insert("max-q", 63);
            break;
        }
        default:
            break;
    }

    return params;
}

int QnTranscoder::setVideoCodec(
    AVCodecID codec,
    TranscodeMethod method,
    Qn::StreamQuality quality,
    const QSize& resolution,
    int bitrate,
    QnCodecParams::Value params )
{
    if (params.isEmpty())
        params = suggestMediaStreamParams(codec, quality);

    QnFfmpegVideoTranscoder* ffmpegTranscoder;
    m_videoCodec = codec;
    switch (method)
    {
        case TM_DirectStreamCopy:
            m_vTranscoder.reset();
            break;
        case TM_FfmpegTranscode:
        {
            ffmpegTranscoder = new QnFfmpegVideoTranscoder(
                m_decoderConfig,
                m_metrics,
                codec);

            ffmpegTranscoder->setOutputResolutionLimit(resolution);
            ffmpegTranscoder->setBitrate(bitrate);
            ffmpegTranscoder->setParams(params);
            ffmpegTranscoder->setQuality(quality);
            ffmpegTranscoder->setUseRealTimeOptimization(m_useRealTimeOptimization);
            auto filterChain = QnImageFilterHelper::createFilterChain(m_transcodingSettings);
            ffmpegTranscoder->setFilterChain(filterChain);

            if (codec != AV_CODEC_ID_H263P && codec != AV_CODEC_ID_MJPEG) {
                // H263P and MJPEG codecs have bug for multi thread encoding in current ffmpeg version
                bool isAtom = getCPUString().toLower().contains(QLatin1String("atom"));
                if (isAtom || resolution.height() >= 1080)
                    ffmpegTranscoder->setUseMultiThreadEncode(true);
            }
            m_vTranscoder = QSharedPointer<QnFfmpegVideoTranscoder>(ffmpegTranscoder);
            break;
        }
            /*
        case TM_QuickSyncTranscode:
            m_vTranscoder = QnVideoTranscoderPtr(new QnQuickSyncTranscoder(codec));
            break;
            */
        case TM_OpenCLTranscode:
            NX_ERROR(this, "OpenCL transcoding is not implemented.");
            return OperationResult::Error;
        default:
            NX_ERROR(this, "Unknown transcoding method.");
            return OperationResult::Error;
    }
    return OperationResult::Success;
}

void QnTranscoder::setUseRealTimeOptimization(bool value)
{
    m_useRealTimeOptimization = value;
    if (m_vTranscoder)
        m_vTranscoder->setUseRealTimeOptimization(value);
}

QnTranscoder::OperationResult QnTranscoder::setAudioCodec(AVCodecID codec, TranscodeMethod method)
{
    m_audioCodec = codec;
    switch (method)
    {
        case TM_DirectStreamCopy:
            m_aTranscoder = QnAudioTranscoderPtr();
            break;
        case TM_FfmpegTranscode:
            m_aTranscoder = QnAudioTranscoderPtr(new QnFfmpegAudioTranscoder(codec));
            break;
            /*
        case TM_QuickSyncTranscode:
            m_vTranscoder = QnVideoTranscoderPtr(new QnQuickSyncTranscoder(codec));
            break;
            */
        case TM_OpenCLTranscode:
            m_lastErrMessage = tr("OpenCLTranscode is not implemented.");
            return OperationResult::Error;
        default:
            m_lastErrMessage = tr("Unknown transcode method");
            return OperationResult::Error;
    }
    return OperationResult::Success;
}

int QnTranscoder::openAndTranscodeDelayedData()
{
    const auto& video = m_delayedVideoQueue.isEmpty() ?
        QnConstCompressedVideoDataPtr() : m_delayedVideoQueue.first();
    const auto& audio = m_delayedAudioQueue.isEmpty() ?
        QnConstCompressedAudioDataPtr() : m_delayedAudioQueue.first();

    if (m_beforeOpenCallback)
        m_beforeOpenCallback(this, video, audio);

    int errCode = open(video, audio);
    if (errCode != 0)
    {
        NX_WARNING(this, "Failed to open transcoder, error code: %1", errCode);
        return errCode;
    }

    errCode = OperationResult::Success;
    while (!m_delayedVideoQueue.isEmpty())
    {
        errCode = transcodePacketInternal(m_delayedVideoQueue.dequeue());
        if (errCode != 0)
        {
            NX_WARNING(this, "Failed to transcode video packet, error code: %1", errCode);
            return errCode;
        }
    }
    while (!m_delayedAudioQueue.isEmpty())
    {
        errCode = transcodePacketInternal(m_delayedAudioQueue.dequeue());
        if (errCode != 0)
        {
            NX_WARNING(this, "Failed to transcode audio packet, error code: %1", errCode);
            return errCode;
        }
    }
    return 0;
}

int QnTranscoder::transcodePacket(const QnConstAbstractMediaDataPtr& media, nx::utils::ByteArray* const result)
{
    m_internalBuffer.clear();
    m_outputPacketSize.clear();
    if (media->dataType == QnAbstractMediaData::EMPTY_DATA)
    {
        finalize(result);

        if (++m_eofCounter >= 3)
            return -8; // EOF reached
        else
            return 0;
    }
    else if (media->dataType != QnAbstractMediaData::VIDEO && media->dataType != QnAbstractMediaData::AUDIO)
        return 0; // transcode only audio and video, skip packet

    m_eofCounter = 0;

    int errCode = OperationResult::Success;
    bool doTranscoding = true;
    static const size_t kMaxDelayedQueueSize = 60;

    if (result)
        result->clear();

    if (!m_initialized)
    {
        if (media->dataType == QnAbstractMediaData::VIDEO)
            m_delayedVideoQueue << std::dynamic_pointer_cast<const QnCompressedVideoData> (media);
        else
            m_delayedAudioQueue << std::dynamic_pointer_cast<const QnCompressedAudioData> (media);
        doTranscoding = false;
        if ((m_videoCodec != AV_CODEC_ID_NONE || m_beforeOpenCallback) && m_delayedVideoQueue.isEmpty()
            && m_delayedAudioQueue.size() < (int)kMaxDelayedQueueSize)
        {
            return 0; // not ready to init
        }
        if (m_audioCodec != AV_CODEC_ID_NONE && m_delayedAudioQueue.isEmpty()
            && m_delayedVideoQueue.size() < (int)kMaxDelayedQueueSize)
        {
            return 0; // not ready to init
        }
        // Ready to initialize transcoder.
        errCode = openAndTranscodeDelayedData();
        if (errCode != 0)
        {
            NX_WARNING(this, "Failed to transcode delayed data, error code: %1", errCode);
            return errCode;
        }
    }

    if ((media->dataType == QnAbstractMediaData::AUDIO && !m_initializedAudio) ||
        (media->dataType == QnAbstractMediaData::VIDEO && !m_initializedVideo))
    {
        return OperationResult::Success;
    }

    if (doTranscoding)
    {
        errCode = transcodePacketInternal(media);
        if (errCode != 0)
            return errCode;
    }

    if (result)
        result->write(m_internalBuffer.data(), m_internalBuffer.size());

    return OperationResult::Success;
}

int QnTranscoder::finalize(nx::utils::ByteArray* const result)
{
    m_internalBuffer.clear();
    if (!m_initialized)
    {
        int errCode = openAndTranscodeDelayedData();
        if (errCode != 0)
        {
            NX_WARNING(this, "Failed to transcode delayed data on finalize, error code: %1",
                errCode);
            return errCode;
        }
    }

    if (!m_initialized)
        return OperationResult::Success;

    finalizeInternal(result);
    if (result)
        result->write(m_internalBuffer.data(), m_internalBuffer.size());
    m_initialized = false;
    return OperationResult::Success;
}

void QnTranscoder::flush(nx::utils::ByteArray* const result)
{
    if (result)
        result->write(m_internalBuffer.data(), m_internalBuffer.size());
}

bool QnTranscoder::addTag( const QString& /*name*/, const QString& /*value*/ )
{
    return false;
}

QString QnTranscoder::getLastErrorMessage() const
{
    return m_lastErrMessage;
}

int QnTranscoder::writeBuffer(const char* data, int size)
{
    m_internalBuffer.write(data,size);
    if (m_packetizedMode)
        m_outputPacketSize << size;
    return size;
}

void QnTranscoder::setPacketizedMode(bool value)
{
    m_packetizedMode = value;
}

const QVector<int>& QnTranscoder::getPacketsSize()
{
    return m_outputPacketSize;
}

void QnTranscoder::setTranscodingSettings(const QnLegacyTranscodingSettings& settings)
{
    m_transcodingSettings = settings;
}

void QnTranscoder::setBeforeOpenCallback(BeforeOpenCallback callback)
{
    m_beforeOpenCallback = std::move(callback);
}
