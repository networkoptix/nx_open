// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <export/signer.h>
#include <nx/media/ffmpeg/frame_info.h>
#include <nx/utils/cryptographic_hash.h>

#include "decoders/video/ffmpeg_video_decoder.h"
#include "transcoder.h"

class QnLicensePool;
class QnFfmpegVideoTranscoder;
class QnFfmpegAudioTranscoder;

class NX_VMS_COMMON_API QnFfmpegTranscoder
{
public:
    static const int MTU_SIZE = 1412;
    enum TranscodeMethod {TM_DirectStreamCopy, TM_FfmpegTranscode};

    struct Config
    {
        bool computeSignature = false;
        bool useAbsoluteTimestamp = false;
        bool keepOriginalTimestamps = false;
        DecoderConfig decoderConfig;
    };

public:
    QnFfmpegTranscoder(const Config& config, nx::metric::Storage* metrics);
    ~QnFfmpegTranscoder();

    /*
    * Set ffmpeg container by name
    * @return Returns true if no error or false on error
    */
    bool setContainer(const QString& value);
    void setFormatOption(const QString& option, const QString& value);
    AVCodecParameters* getVideoCodecParameters() const;
    AVCodecParameters* getAudioCodecParameters() const;
    AVFormatContext* getFormatContext() const { return m_formatCtx; }

    /*
    * Prepare to transcode. If 'direct stream copy' is used, function got not empty video and audio data
    * Destination codecs MUST be used from source data codecs. If 'direct stream copy' is false, video or audio may be empty
    * @return Returns true if no error or false otherwise
    */
    bool open(const QnConstCompressedVideoDataPtr& video, const QnConstCompressedAudioDataPtr& audio);

    //!Adds tag to the file. Maximum length of tags and allowed names are format dependent
    /*!
        This implementation always returns \a false
        \return true on success
    */
    bool addTag(const char* name, const char* value);

    void setInMiddleOfStream(bool value) { m_inMiddleOfStream = value; }
    bool inMiddleOfStream() const { return m_inMiddleOfStream; }
    void setStartTimeOffset(qint64 value) { m_startTimeOffset = value; }

    QByteArray getSignature(QnLicensePool* licensePool, const nx::Uuid& serverId);

    struct PacketTimestamp
    {
        uint64_t ntpTimestamp = 0;
        uint32_t rtpTimestamp = 0;
    };
    PacketTimestamp getLastPacketTimestamp() const { return m_lastPacketTimestamp; }
    void setRtpMtu(int mtu) { m_rtpMtu = mtu; }
    void setSeeking() { m_isSeeking = true; };

    void setTranscodingSettings(const QnLegacyTranscodingSettings& settings);
    void setSourceResolution(const QSize& resolution);
    void setUseRealTimeOptimization(bool value);

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

    /*
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
    //!Flushes codec buffer. Use after \a open() call to flush muxed header.
    void flush(nx::utils::ByteArray* const result);

    /**
     * Return description of the last error code
     */
    QString getLastErrorMessage() const;

    // For internal use only, move to protected!
    int writeBuffer(const char* data, int size);
    void setPacketizedMode(bool value);
    const QVector<int>& getPacketsSize();

private:
    int transcodePacketInternal(const QnConstAbstractMediaDataPtr& media);
    int finalizeInternal(nx::utils::ByteArray* const result);

private:
    bool isCodecSupported(AVCodecID id) const;
    //friend qint32 ffmpegWritePacket(void *opaque, quint8* buf, int size);
    AVIOContext* createFfmpegIOContext();
    void closeFfmpegContext();
    int muxPacket(const QnConstAbstractMediaDataPtr& packet);
    AVPixelFormat getPixelFormatJpeg(const QnConstCompressedVideoDataPtr& video);
    bool handleSeek(const QnConstAbstractMediaDataPtr& packet);
    int openAndTranscodeDelayedData();

private:
    Config m_config;
    std::unique_ptr<QnFfmpegVideoTranscoder> m_vTranscoder;
    std::unique_ptr<QnFfmpegAudioTranscoder> m_aTranscoder;
    AVCodecID m_videoCodec = AV_CODEC_ID_NONE;
    AVCodecID m_audioCodec = AV_CODEC_ID_NONE;
    nx::utils::ByteArray m_internalBuffer;
    QVector<int> m_outputPacketSize;

private:
    bool m_initialized = false;
    //! Make sure to correctly fill these member variables in overridden open() function.
    bool m_initializedAudio  =false;    // Incoming audio packets will be ignored.
    bool m_initializedVideo = false;    // Incoming video packets will be ignored.
    nx::metric::Storage* m_metrics = nullptr;

private:
    QString m_lastErrMessage;
    QQueue<QnConstCompressedVideoDataPtr> m_delayedVideoQueue;
    QQueue<QnConstCompressedAudioDataPtr> m_delayedAudioQueue;
    int m_eofCounter = 0;
    bool m_packetizedMode = false;
    QnLegacyTranscodingSettings m_transcodingSettings;
    bool m_useRealTimeOptimization = false;
    BeforeOpenCallback m_beforeOpenCallback;

private:
    QSize m_sourceResolution;
    MediaSigner m_mediaSigner;
    AVCodecParameters* m_videoCodecParameters = nullptr;
    AVCodecParameters* m_audioCodecParameters = nullptr;
    AVFormatContext* m_formatCtx = nullptr;

    QString m_container;
    qint64 m_baseTime = AV_NOPTS_VALUE;
    PacketTimestamp m_lastPacketTimestamp;
    bool m_inMiddleOfStream = false;
    qint64 m_startTimeOffset = 0;
    int m_rtpMtu = MTU_SIZE;
    bool m_isSeeking = false;
};
