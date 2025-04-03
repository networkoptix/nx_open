// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QQueue>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <decoders/video/ffmpeg_video_decoder.h>
#include <export/signer.h>
#include <nx/core/transcoding/filters/legacy_transcoding_settings.h>
#include <nx/media/audio_data_packet.h>
#include <nx/media/ffmpeg/frame_info.h>
#include <nx/utils/cryptographic_hash.h>
#include <nx/utils/move_only_func.h>
#include <transcoding/ffmpeg_muxer.h>
#include <transcoding/media_transcoder.h>
#include <transcoding/transcoding_utils.h>

class QnLicensePool;

class NX_VMS_COMMON_API QnFfmpegTranscoder
{
public:
    static const int MTU_SIZE = 1412;
    enum TranscodeMethod {TM_DirectStreamCopy, TM_FfmpegTranscode};

    struct Config
    {
        MediaTranscoder::Config mediaTranscoderConfig;
        FfmpegMuxer::Config muxerConfig;
        bool utcTimestamps = false;
    };

public:
    QnFfmpegTranscoder(const Config& config, nx::metric::Storage* metrics);
    ~QnFfmpegTranscoder();

    FfmpegMuxer& muxer() { return m_muxer; };
    const FfmpegMuxer& muxer() const { return m_muxer; };
    /*
    * Prepare to transcode. If 'direct stream copy' is used, function got not empty video and audio data
    * Destination codecs MUST be used from source data codecs. If 'direct stream copy' is false, video or audio may be empty
    * @return Returns true if no error or false otherwise
    */
    bool open(const QnConstCompressedVideoDataPtr& video, const QnConstCompressedAudioDataPtr& audio);

    // Direct stream copy of one stream only to support usage as muxer in QnUniversalRtpEncoder.
    bool open(const AVCodecParameters* codecParameters);
    void setTranscodingSettings(const QnLegacyTranscodingSettings& settings);

    using BeforeOpenCallback = nx::utils::MoveOnlyFunc<void(
        QnFfmpegTranscoder* transcoder,
        const QnConstCompressedVideoDataPtr & video,
        const QnConstCompressedAudioDataPtr & audio)>;
    void setBeforeOpenCallback(BeforeOpenCallback callback);

    /*
    * Set ffmpeg video codec and params
    * @return Returns OperationResult::Success if no error or error code otherwise
    * @param codec codec to transcode
    * @param method how to transcode: no transcode, software, GPU e.t.c
    * @resolution output resolution. If not valid, than source resolution will be used. Not used if transcode method TM_NoTranscode
    * @param bitrate Bitrate after transcode. By default bitrate is autodetected. Not used if transcode method TM_NoTranscode
    * @param addition codec params. Not used if transcode method TM_NoTranscode
    */
    bool setVideoCodec(
        AVCodecID codec,
        TranscodeMethod method,
        Qn::StreamQuality quality,
        const QSize& sourceResolition,
        const QSize& resolution = QSize(),
        int bitrate = -1,
        QnCodecParams::Value params = QnCodecParams::Value());

    /*
    * Set ffmpeg audio codec and params
    * @return Returns OperationResult::Success if no error or error code otherwise
    * @param codec codec to transcode
    * @param directStreamCopy if true - do not transcode audio data, only put it to destination container
    * @param bitrate Bitrate after transcode. By default bitrate is autodetected
    * @param addition codec params. Not used if directStreamCopy = true
    */
    bool setAudioCodec(AVCodecID codec, TranscodeMethod method);

    /*p
    * Transcode media data and write it to specified nx::utils::ByteArray
    * @param result transcoded data block. If NULL, only decoding is done
    * @return Returns OperationResult::Success if no error or error code otherwise
    */
    int transcodePacket(const QnConstAbstractMediaDataPtr& media, nx::utils::ByteArray* const result);
    //!Flushes codec buffer and finalizes stream. No \a QnFfmpegTranscoder::transcodePacket is allowed after this call
    /*
    * @return Returns OperationResult::Success if no error or error code otherwise
    */
    int finalize(nx::utils::ByteArray* const result);

private:
    bool transcodePacketInternal(const QnConstAbstractMediaDataPtr& media);
    bool finalizeInternal(nx::utils::ByteArray* const result);
    bool isCodecSupported(AVCodecID id) const;
    int openAndTranscodeDelayedData();

private:
    MediaTranscoder m_mediaTranscoder;
    FfmpegMuxer m_muxer;
    AVCodecID m_videoCodec = AV_CODEC_ID_NONE;
    AVCodecID m_audioCodec = AV_CODEC_ID_NONE;

    bool m_initialized = false;

    QQueue<QnConstCompressedVideoDataPtr> m_delayedVideoQueue;
    QQueue<QnConstCompressedAudioDataPtr> m_delayedAudioQueue;
    int m_eofCounter = 0;
    BeforeOpenCallback m_beforeOpenCallback;
    const Config m_config;
};
