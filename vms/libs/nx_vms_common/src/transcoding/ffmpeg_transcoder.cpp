// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_transcoder.h"

extern "C" {
#include <libavutil/opt.h>
}

#include <core/resource/avi/avi_archive_metadata.h>
#include <nx/codec/jpeg/jpeg_utils.h>
#include <nx/media/ffmpeg/av_packet.h>
#include <nx/media/ffmpeg/ffmpeg_utils.h>
#include <nx/media/utils.h>
#include <nx/ranges.h>
#include <nx/utils/log/log.h>

using namespace nx::vms::api;

namespace {

bool isMoovFormat(const QString& container)
{
    return container.compare("mp4", Qt::CaseInsensitive) == 0
        || container.compare("ismv", Qt::CaseInsensitive) == 0;
}

// Container-specific ordered audio codec preferences.
// This mapping is needed because, as of this writing, no libav/libavformat
// function is known to return a ready-to-use list of audio codecs supported by a
// container in the priority order required by this code path. The order matters:
// multiple codecs may be reported as supported by the muxer, but that does not
// guarantee equivalent playback behavior. For example, some players may accept MP3
// in MP4/ISMV nominally, yet fail to play audio, while AAC plays correctly.
// Therefore this table encodes both compatibility and preference.
const std::map<QString, std::vector<AVCodecID>> kAudioFallbacksByContainer = {
    {"mp4", {AV_CODEC_ID_AAC, AV_CODEC_ID_MP3}},
    {"ismv", {AV_CODEC_ID_AAC, AV_CODEC_ID_MP3}},
    {"webm", {AV_CODEC_ID_OPUS, AV_CODEC_ID_VORBIS}},
    {"mpegts", {AV_CODEC_ID_MP2, AV_CODEC_ID_AAC, AV_CODEC_ID_MP3}},
    {"ts", {AV_CODEC_ID_MP2, AV_CODEC_ID_AAC, AV_CODEC_ID_MP3}},
};

// Fallbacks for other containers.
const std::vector kAudioFallbacks = {
    AV_CODEC_ID_MP3,
    AV_CODEC_ID_VORBIS,
};

QString codecNameForId(AVCodecID c)
{
    return QString::fromLatin1(avcodec_get_name(c));
}

} // namespace

bool QnFfmpegTranscoder::setVideoCodec(
    AVCodecID codec,
    TranscodeMethod method,
    Qn::StreamQuality quality,
    const QSize& sourceResolition,
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
            m_mediaTranscoder.setVideoCodec(codec, quality, sourceResolition, resolution, bitrate, params);
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

        return 0;
    }

    if (media->dataType != QnAbstractMediaData::VIDEO && media->dataType != QnAbstractMediaData::AUDIO)
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

void QnFfmpegTranscoder::setTranscodingSettings(const nx::core::transcoding::Settings& settings)
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
    m_muxer(config.muxerConfig),
    m_config(config)
{
    NX_DEBUG(this, "Created new ffmpeg transcoder. Total transcoder count %1",
        QnFfmpegTranscoder_count.fetchAndAddOrdered(1) + 1);
}

QnFfmpegTranscoder::~QnFfmpegTranscoder()
{
    NX_DEBUG(this, "Destroying ffmpeg transcoder. Total transcoder count %1",
        QnFfmpegTranscoder_count.fetchAndAddOrdered(-1) - 1);
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

    if (!m_muxer.open())
        return false;

    m_initialized = true;
    return true;
}

bool QnFfmpegTranscoder::open(const QnConstCompressedVideoDataPtr& video, const QnConstCompressedAudioDataPtr& audio)
{
    if (!video)
        m_mediaTranscoder.resetVideo();
    else if (!NX_ASSERT(video->context))
        return false;

    if (!audio)
        m_mediaTranscoder.resetAudio();
    else if (!NX_ASSERT(audio->context))
        return false;


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
        // NOTE: #skolesnik If m_audioCodec was assigned explicitly via setAudioCodec
        // before entering this block, this probe path is skipped, and the
        // container-specific codec checks below are not performed. As of this
        // writing, setAudioCodec itself does not perform such checks either,
        // regardless of the current m_muxer state.
        if (AV_CODEC_ID_PROBE == m_audioCodec)
        {
            const auto isCodecSupported =
                [this](const AVCodecID c)
                {
                    return m_muxer.isCodecSupported(c);
                };

            if (const auto sourceCodec = audio->context->getCodecId(); isCodecSupported(sourceCodec))
            {
                // This check itself is not sufficient for direct stream copy.
                // Additional container/bitstream compatibility checks should be performed here
                // when such logic is implemented.
                setAudioCodec(sourceCodec, TM_DirectStreamCopy);
            }
            else
            {
                const auto container = m_muxer.container().toLower();
                const std::vector supportedCodecs =
                    kAudioFallbacksByContainer.contains(container)
                        ? kAudioFallbacksByContainer.at(container)
                        : kAudioFallbacks
                    | std::views::filter(isCodecSupported)
                    | nx::ranges::to<std::vector>();

                if (supportedCodecs.empty())
                {
                    m_audioCodec = AV_CODEC_ID_NONE;
                    m_mediaTranscoder.resetAudio();
                    NX_WARNING(
                        this, "No supported audio codecs for container %1", m_muxer.container());
                }
                else
                {
                    NX_DEBUG(this,
                        "Supported audio codecs for container %1: %2",
                        m_muxer.container(),
                        supportedCodecs
                            | std::views::transform(codecNameForId)
                            | nx::ranges::to<std::vector>());

                    const auto fallback = supportedCodecs.front();
                    NX_DEBUG(this,
                        "Selecting fallback audio codec %1 instead of %2 for container %3",
                        codecNameForId(fallback),
                        codecNameForId(sourceCodec),
                        m_muxer.container());
                    setAudioCodec(fallback, TM_FfmpegTranscode);
                }
            }

            if (AV_CODEC_ID_MP3 == m_audioCodec
                && audio->context->getSampleRate() < kMinMp4Mp3SampleRate
                && isMoovFormat(m_muxer.container()))
            {
                NX_DEBUG(this, "Resampling audio codec %1", codecNameForId(m_audioCodec));
                m_mediaTranscoder.setAudioSampleRate(kMinMp4Mp3SampleRate);
            }
        }

        if (m_mediaTranscoder.audioTranscoder() && !m_mediaTranscoder.openAudio(audio))
        {
            NX_WARNING(this, "Failed to open audio codec %1: stream disabled.", codecNameForId(m_audioCodec));
            m_mediaTranscoder.resetAudio();
            m_audioCodec = AV_CODEC_ID_NONE;
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
            if (m_mediaTranscoder.audioTranscoder()->getCodecParameters())
            {
                avcodec_parameters_copy(
                    codecpar,
                    m_mediaTranscoder.audioTranscoder()->getCodecParameters());
            }
        }
        else
        {
            if (audio->context)
                avcodec_parameters_copy(codecpar, audio->context->getAvCodecParameters());
        }
        if (!m_muxer.addAudio(codecpar))
            return false;
    }

    std::optional<QnAviArchiveMetadata> metadata;
    {
        metadata = QnAviArchiveMetadata();
        if (video)
            metadata->startTimeMs = video->timestamp / 1000;
        else if (audio)
            metadata->startTimeMs = audio->timestamp / 1000;
        metadata->version = QnAviArchiveMetadata::kMetadataStreamVersion_4;

        if (m_timeZone)
        {
            metadata->timeZoneOffset = m_timeZone->timeZoneOffsetMs;
            metadata->timeZoneId = m_timeZone->timeZoneId;
        }
    }

    if (!m_muxer.open(metadata))
        return false;

    m_initialized = true;
    return true;
}

void QnFfmpegTranscoder::setTimeZone(const nx::vms::api::ServerTimeZoneInformation& value)
{
    m_timeZone = value;
}

bool QnFfmpegTranscoder::transcodePacketInternal(
    const QnConstAbstractMediaDataPtr& media)
{
    return m_mediaTranscoder.transcodePacket(media,
        [this](const QnConstAbstractMediaDataPtr& media)
        {
            m_muxer.process(media);
        });
}
