// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_transcoder.h"

#include <QtCore/QThread>

#include <nx/media/ffmpeg/ffmpeg_utils.h>
#include <nx/utils/log/log.h>
#include <transcoding/filters/abstract_image_filter.h>
#include <transcoding/filters/filter_helper.h>

MediaTranscoder::MediaTranscoder(const Config& config, nx::metric::Storage* metrics):
    m_config(config),
    m_metrics(metrics)
{
    QThread::currentThread()->setPriority(QThread::LowPriority);
}

bool MediaTranscoder::setVideoCodec(
    AVCodecID codec,
    Qn::StreamQuality quality,
    const QSize& sourceResolution,
    const QSize& resolution,
    int bitrate,
    QnCodecParams::Value params)
{
    if (params.isEmpty())
        params = suggestMediaStreamParams(codec, quality);

    QnFfmpegVideoTranscoder::Config config;
    config.sourceResolution = sourceResolution;
    config.outputResolutionLimit = resolution;
    config.bitrate = bitrate;
    config.params = params;
    config.quality = quality;
    config.useRealTimeOptimization = m_config.useRealTimeOptimization;
    config.decoderConfig = m_config.decoderConfig;
    config.targetCodecId = codec;
    config.useMultiThreadEncode = nx::transcoding::useMultiThreadEncode(codec, resolution);
    config.rtpContatiner = m_config.rtpContatiner;

    return setVideoCodec(config, m_transcodingSettings);
}

bool MediaTranscoder::setVideoCodec(
    QnFfmpegVideoTranscoder::Config config,
    QnLegacyTranscodingSettings transcodingSettings)
{
    m_vTranscoder = std::make_unique<QnFfmpegVideoTranscoder>(config, m_metrics);
    auto filterChain = QnImageFilterHelper::createFilterChain(transcodingSettings);
    m_vTranscoder->setFilterChain(filterChain);
    return true;
}

bool MediaTranscoder::setAudioCodec(AVCodecID codec)
{
    QnFfmpegAudioTranscoder::Config config;
    config.targetCodecId = codec;
    m_aTranscoder = std::make_unique<QnFfmpegAudioTranscoder>(config);
    return true;
}

void MediaTranscoder::setTranscodingSettings(const QnLegacyTranscodingSettings& settings)
{
    m_transcodingSettings = settings;
}

bool MediaTranscoder::openVideo(const QnConstCompressedVideoDataPtr& video)
{
    if (!m_vTranscoder)
        return false;

    return m_vTranscoder->open(video);
}

bool MediaTranscoder::openAudio(const QnConstCompressedAudioDataPtr& audio)
{
    if (!m_aTranscoder)
        return false;

    return m_aTranscoder->open(audio);
}

void MediaTranscoder::resetVideo()
{
    m_vTranscoder.reset();
}

void MediaTranscoder::resetAudio()
{
    m_aTranscoder.reset();
}

void MediaTranscoder::setAudioSampleRate(int value)
{
    if (m_aTranscoder)
        m_aTranscoder->setSampleRate(value);
}

QnFfmpegVideoTranscoder* MediaTranscoder::videoTranscoder()
{
    return m_vTranscoder.get();
}

QnFfmpegAudioTranscoder* MediaTranscoder::audioTranscoder()
{
    return m_aTranscoder.get();
}

bool MediaTranscoder::transcodePacket(
    const QnConstAbstractMediaDataPtr& media, const OnDataHandler& handler)
{
    AbstractCodecTranscoder* transcoder = nullptr;
    if (dynamic_cast<const QnCompressedVideoData*>(media.get()))
        transcoder = m_vTranscoder.get();
    else
        transcoder = m_aTranscoder.get();

    if (transcoder)
    {
        /* TODO
         * Summary: API Transcoder::transcodePacket() is a bit confused and should be rewritten
         * into FFmpeg-like sendFrame() / receivePacket() pattern.
         *
         * Details: Transcoder's API should provide a methods for:
         * 1) send frame into transcoder;
         * 2) get next frame from transcoder (0, 1 or more frames can be received per frame sent
         * due to an internal decoder reasons or due to resampling for audio of fps change for
         * video);
         * 3) flush transcoder on transcoding stop;
         * Current video and audio transcoders API use a null pointer as an output for a 2) and 3)
         * methods. But audio transcoder implementation treat a null output as a method 2), and
         * have no API for method 3). And video transcoder implementation treat a null output as a
         * method 3), and have no API for method 2). That's why we break transcoding cycle only
         * for video.
         * */
        QnConstAbstractMediaDataPtr packet = media;
        do
        {
            QnAbstractMediaDataPtr transcodedPacket;
            int errCode = transcoder->transcodePacket(packet, &transcodedPacket);
            if (errCode != 0)
            {
                NX_DEBUG(this, "Transcoding error: %1", nx::media::ffmpeg::avErrorToString(errCode));
                return false;
            }
            if (!transcodedPacket || transcodedPacket->dataSize() == 0)
                break;
            handler(transcodedPacket);
            packet.reset();
        } while (transcoder == m_aTranscoder.get());
    }
    else
    {
        // direct stream copy
        if (media && media->dataSize() > 0)
            handler(media);
    }
    return true;
}

bool finalizeTranscoder(
    AbstractCodecTranscoder* transcoder, const MediaTranscoder::OnDataHandler& handler)
{
    if (!transcoder)
        return true;

    QnAbstractMediaDataPtr transcodedData;
    do
    {
        transcodedData.reset();
        // Transcode media.
        int errCode = transcoder->transcodePacket(QnConstAbstractMediaDataPtr(), &transcodedData);
        if (errCode != 0)
            return false;
        if (transcodedData && transcodedData->dataSize() > 0)
            handler(QnConstAbstractMediaDataPtr(transcodedData));
    } while (transcodedData);

    return true;
}

bool MediaTranscoder::finalize(const OnDataHandler& handler)
{
    if (!finalizeTranscoder(m_vTranscoder.get(), handler))
        return false;
    if (!finalizeTranscoder(m_aTranscoder.get(), handler))
        return false;

    return true;
}
