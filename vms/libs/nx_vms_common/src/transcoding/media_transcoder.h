// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QQueue>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <decoders/video/ffmpeg_video_decoder.h>
#include <nx/core/transcoding/filters/legacy_transcoding_settings.h>
#include <nx/media/audio_data_packet.h>
#include <transcoding/ffmpeg_audio_transcoder.h>
#include <transcoding/ffmpeg_video_transcoder.h>
#include <transcoding/transcoding_utils.h>

class NX_VMS_COMMON_API MediaTranscoder
{
public:
    struct Config
    {
        bool keepOriginalTimestamps = false;
        bool useRealTimeOptimization = false;
        // Set this to true if stream will mux into RTP.
        bool rtpContatiner = false;
        DecoderConfig decoderConfig;
    };

public:
    MediaTranscoder(const Config& config, nx::metric::Storage* metrics);

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
        Qn::StreamQuality quality,
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
    bool setAudioCodec(AVCodecID codec);

    /*
    * Prepare to transcode. If 'direct stream copy' is used, function got not empty video and audio data
    * Destination codecs MUST be used from source data codecs. If 'direct stream copy' is false, video or audio may be empty
    * @return Returns true if no error or false otherwise
    */
    bool openVideo(const QnConstCompressedVideoDataPtr& video);
    bool openAudio(const QnConstCompressedAudioDataPtr& audio);

    // This Handler is sync, it will call in the same thread.
    using OnDataHandler = std::function<void (const QnConstAbstractMediaDataPtr& packet)>;
    bool transcodePacket(const QnConstAbstractMediaDataPtr& media, const OnDataHandler& handler);
    bool finalize(const OnDataHandler& handler);

    // For compatibility with QnFfmpegTranscoder.
    QnFfmpegVideoTranscoder* videoTranscoder();
    QnFfmpegAudioTranscoder* audioTranscoder();
    void resetVideo();
    void resetAudio();
    void setAudioSampleRate(int value);
    void setTranscodingSettings(const QnLegacyTranscodingSettings& settings);
    void setSourceResolution(const QSize& resolution);

private:
    const Config m_config;
    nx::metric::Storage* m_metrics = nullptr;
    std::unique_ptr<QnFfmpegVideoTranscoder> m_vTranscoder;
    std::unique_ptr<QnFfmpegAudioTranscoder> m_aTranscoder;
    QnLegacyTranscodingSettings m_transcodingSettings;
    QSize m_sourceResolution;
};
