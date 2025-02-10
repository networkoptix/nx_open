// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_transcoder.h"

extern "C" {
#include <libavutil/opt.h>
}

#include <nx/codec/jpeg/jpeg_utils.h>
#include <nx/media/ffmpeg/av_packet.h>
#include <nx/media/ffmpeg/ffmpeg_utils.h>
#include <nx/media/utils.h>
#include <nx/utils/log/log.h>
#include <utils/media/ffmpeg_io_context.h>

using namespace nx::vms::api;

static bool isMoovFormat(const QString& container)
{
    return container.compare("mp4", Qt::CaseInsensitive) == 0
        || container.compare("ismv", Qt::CaseInsensitive) == 0;
}

bool QnFfmpegTranscoder::setVideoCodec(
    AVCodecID codec,
    TranscodeMethod method,
    Qn::StreamQuality quality,
    const QSize& resolution,
    int bitrate,
    QnCodecParams::Value params)
{
    m_videoCodec = codec;
    switch (method)
    {
        case TM_DirectStreamCopy:
            m_mediaTranscoder.resetVideo();
            break;
        case TM_FfmpegTranscode:
            m_mediaTranscoder.setVideoCodec(codec, quality, resolution, bitrate, params);
            break;
        default:
            NX_ERROR(this, "Unknown transcoding method.");
            return false;
    }
    return true;
}

bool QnFfmpegTranscoder::setAudioCodec(AVCodecID codec, TranscodeMethod method)
{
    m_audioCodec = codec;
    switch (method)
    {
        case TM_DirectStreamCopy:
            m_mediaTranscoder.resetAudio();
            break;
        case TM_FfmpegTranscode:
            m_mediaTranscoder.setAudioCodec(codec);
            break;
        default:
            return false;
    }
    return true;
}

int QnFfmpegTranscoder::openAndTranscodeDelayedData()
{
    const auto& video = m_delayedVideoQueue.isEmpty() ?
        QnConstCompressedVideoDataPtr() : m_delayedVideoQueue.first();
    const auto& audio = m_delayedAudioQueue.isEmpty() ?
        QnConstCompressedAudioDataPtr() : m_delayedAudioQueue.first();

    if (m_beforeOpenCallback)
        m_beforeOpenCallback(this, video, audio);

    if (!open(video, audio))
    {
        NX_WARNING(this, "Failed to open transcoder");
        return -1;
    }

    while (!m_delayedVideoQueue.isEmpty())
    {
        if (!transcodePacketInternal(m_delayedVideoQueue.dequeue()))
        {
            NX_WARNING(this, "Failed to transcode video packet");
            return -1;
        }
    }
    while (!m_delayedAudioQueue.isEmpty())
    {
        if (!transcodePacketInternal(m_delayedAudioQueue.dequeue()))
        {
            NX_WARNING(this, "Failed to transcode audio packet");
            return -1;
        }
    }
    return 0;
}

int QnFfmpegTranscoder::transcodePacket(const QnConstAbstractMediaDataPtr& media, nx::utils::ByteArray* const result)
{
    if (result)
        result->clear();

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

    int errCode = true;
    bool doTranscoding = true;
    static const size_t kMaxDelayedQueueSize = 60;

    if (!m_initialized)
    {
        if (media->dataType == QnAbstractMediaData::VIDEO)
            m_delayedVideoQueue << std::dynamic_pointer_cast<const QnCompressedVideoData>(media);
        else
            m_delayedAudioQueue << std::dynamic_pointer_cast<const QnCompressedAudioData>(media);
        doTranscoding = false;
        if ((m_videoCodec != AV_CODEC_ID_NONE || m_beforeOpenCallback) && m_delayedVideoQueue.isEmpty()
            && m_delayedAudioQueue.size() < (int)kMaxDelayedQueueSize)
        {
            return 0; // not ready to init
        }
        if (m_audioCodec != AV_CODEC_ID_NONE && m_delayedAudioQueue.isEmpty()
            && m_delayedVideoQueue.size() < (int)kMaxDelayedQueueSize)
        {
            return 0; // not ready to initQnFfmpegTranscoder
        }
        // Ready to initialize transcoder.
        errCode = openAndTranscodeDelayedData();
        if (errCode != 0)
        {
            NX_WARNING(this, "Failed to transcode delayed data, error code: %1", errCode);
            return errCode;
        }
    }

    if (doTranscoding)
    {
        if (!transcodePacketInternal(media))
            return -1;
    }

    m_muxer.getResult(result);
    return 0;
}

int QnFfmpegTranscoder::finalize(nx::utils::ByteArray* const result)
{
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
        return 0;

    auto handler =
        [this](const QnConstAbstractMediaDataPtr& media)
        {
            m_muxer.process(media);
        };

    if (!m_mediaTranscoder.finalize(handler))
    {
        NX_WARNING(this, "Failed to finalize transcoder");
        return false;
    }

    if (!m_muxer.finalize())
    {
        NX_WARNING(this, "Failed to finalize muxer");
        return false;
    }
    m_muxer.getResult(result);
    m_initialized = false;
    return 0;
}

void QnFfmpegTranscoder::setTranscodingSettings(const QnLegacyTranscodingSettings& settings)
{
    m_mediaTranscoder.setTranscodingSettings(settings);
}

void QnFfmpegTranscoder::setBeforeOpenCallback(BeforeOpenCallback callback)
{
    m_beforeOpenCallback = std::move(callback);
}

static QAtomicInt QnFfmpegTranscoder_count = 0;

QnFfmpegTranscoder::QnFfmpegTranscoder(const Config& config, nx::metric::Storage* metrics):
    m_mediaTranscoder(config.mediaTranscoderConfig, metrics),
    m_muxer(config.muxerConfig)
{
    NX_DEBUG(this, "Created new ffmpeg transcoder. Total transcoder count %1",
        QnFfmpegTranscoder_count.fetchAndAddOrdered(1) + 1);
}

QnFfmpegTranscoder::~QnFfmpegTranscoder()
{
    NX_DEBUG(this, "Destroying ffmpeg transcoder. Total transcoder count %1",
        QnFfmpegTranscoder_count.fetchAndAddOrdered(-1) - 1);
}

void QnFfmpegTranscoder::setSourceResolution(const QSize& resolution)
{
    m_mediaTranscoder.setSourceResolution(resolution);
}

bool QnFfmpegTranscoder::open(const AVCodecParameters* codecParameters)
{
    m_mediaTranscoder.resetVideo();
    m_mediaTranscoder.resetAudio();

    bool result = false;
    if (codecParameters->codec_type == AVMEDIA_TYPE_VIDEO)
        result = m_muxer.addVideo(codecParameters);
    else if (codecParameters->codec_type == AVMEDIA_TYPE_AUDIO)
        result = m_muxer.addAudio(codecParameters);

    if (!result)
        return false;

    return m_muxer.open();
}

bool QnFfmpegTranscoder::open(const QnConstCompressedVideoDataPtr& video, const QnConstCompressedAudioDataPtr& audio)
{
    if (!video)
        m_mediaTranscoder.resetVideo();

    if (!audio)
        m_mediaTranscoder.resetAudio();

    if (video && m_videoCodec != AV_CODEC_ID_NONE)
    {
        CodecParameters codecParameters;
        auto codecpar = codecParameters.getAvCodecParameters();
        codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        codecpar->codec_id = m_videoCodec;
        if (m_mediaTranscoder.videoTranscoder())
        {
            if (!m_mediaTranscoder.openVideo(video))
            {
                NX_WARNING(this, "Can't open video transcoder for RTSP streaming");
                return false;
            }

            if (m_mediaTranscoder.videoTranscoder()->getCodecContext())
            {
                avcodec_parameters_from_context(
                    codecpar, m_mediaTranscoder.videoTranscoder()->getCodecContext());
            }
            codecpar->bit_rate = m_mediaTranscoder.videoTranscoder()->getBitrate();
        }
        else
        {
            if (video->context)
                avcodec_parameters_copy(codecpar, video->context->getAvCodecParameters());

            if (isMoovFormat(m_muxer.container()))
            {
                if (!nx::media::fillExtraDataAnnexB(
                    video.get(),
                    &codecpar->extradata,
                    &codecpar->extradata_size))
                {
                    NX_WARNING(this, "Failed to build extra data");
                }
            }
            // Auto-fill bitrate: 2Mbit for full HD, 1Mbit for 720x768.
            codecpar->bit_rate = codecpar->width * codecpar->height; // Is it necessary?
        }
        if (!m_muxer.addVideo(codecpar))
            return false;
    }

    if (audio)
    {
        if (m_audioCodec == AV_CODEC_ID_PROBE)
        {
            m_audioCodec = AV_CODEC_ID_NONE;
            if (audio->context && m_muxer.isCodecSupported(audio->context->getCodecId()))
            {
                setAudioCodec(audio->context->getCodecId(), TranscodeMethod::TM_DirectStreamCopy);
            }
            else
            {
                static const std::vector<AVCodecID> audioCodecs =
                    {AV_CODEC_ID_MP3, AV_CODEC_ID_VORBIS}; //< Audio codecs to transcode.
                for (const auto& codec: audioCodecs)
                {
                    if (m_muxer.isCodecSupported(codec))
                    {
                        setAudioCodec(codec, TranscodeMethod::TM_FfmpegTranscode);
                        if (isMoovFormat(m_muxer.container())
                            && audio->context->getSampleRate() < kMinMp4Mp3SampleRate
                            && codec == AV_CODEC_ID_MP3)
                        {
                            m_mediaTranscoder.setAudioSampleRate(kMinMp4Mp3SampleRate);
                        }
                        break;
                    }
                }
            }
            NX_DEBUG(this, "Auto select audio codec %1 for format %2",
                m_audioCodec, m_muxer.container());
        }

        if (m_mediaTranscoder.audioTranscoder() && !m_mediaTranscoder.openAudio(audio))
        {
            m_mediaTranscoder.resetAudio();
            m_audioCodec = AV_CODEC_ID_NONE; // can't open transcoder. disable audio
        }
    }

    if (audio && m_audioCodec != AV_CODEC_ID_NONE)
    {
        CodecParameters codecParameters;
        auto codecpar = codecParameters.getAvCodecParameters();

        codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        codecpar->codec_id = m_audioCodec;

        if (m_mediaTranscoder.audioTranscoder())
        {
            if (m_mediaTranscoder.audioTranscoder()->getCodecContext())
            {
                avcodec_parameters_from_context(
                    codecpar,
                    m_mediaTranscoder.audioTranscoder()->getCodecContext());
            }

            codecpar->bit_rate = m_mediaTranscoder.audioTranscoder()->getBitrate();
        }
        else
        {
            if (audio->context)
                avcodec_parameters_copy(codecpar, audio->context->getAvCodecParameters());
        }
        if (!m_muxer.addAudio(codecpar))
            return false;
    }

    if (!m_muxer.open())
        return false;

    m_initialized = true;
    return true;
}

bool QnFfmpegTranscoder::handleSeek(const QnConstAbstractMediaDataPtr& media)
{
    if (m_firstSeek) // TODO bad design?
    {
        m_isSeeking = false;
        m_firstSeek = false;
    }

    if (m_isSeeking)
    {
        if (m_mediaTranscoder.videoTranscoder())
        {
            const auto video = std::dynamic_pointer_cast<const QnCompressedVideoData>(media);
            if (video)
            {
                if (!m_mediaTranscoder.openVideo(video)) //< Works as reopen too.
                {
                    NX_DEBUG(this, "Failed to reopen video transcoder");
                    return false;
                }
                m_isSeeking = false;
            }
        }
    }
    return true;
}

bool QnFfmpegTranscoder::transcodePacketInternal(
    const QnConstAbstractMediaDataPtr& media)
{
    if (!handleSeek(media))
        return false;

    return m_mediaTranscoder.transcodePacket(media,
        [this](const QnConstAbstractMediaDataPtr& media)
        {
            m_muxer.process(media);
        });
}

void QnFfmpegTranscoder::setSeeking()
{
    m_muxer.setSeeking();
    m_isSeeking = true;
};
