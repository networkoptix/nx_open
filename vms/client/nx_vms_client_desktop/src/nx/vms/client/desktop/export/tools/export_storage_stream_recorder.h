// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QTimeZone>

#include <export/signer.h>
#include <nx/core/transcoding/filters/filter_chain.h>
#include <recording/storage_recording_context.h>
#include <recording/stream_recorder.h>
#include <transcoding/ffmpeg_audio_transcoder.h>
#include <transcoding/ffmpeg_video_transcoder.h>
#include <transcoding/timestamp_corrector.h>

namespace nx::vms::client::desktop {

class ExportStorageStreamRecorder:
    public QnStreamRecorder,
    public nx::StorageRecordingContext
{
public:
    ExportStorageStreamRecorder(
        const QnResourcePtr& dev, QnAbstractStreamDataProvider* mediaProvider);
    virtual ~ExportStorageStreamRecorder() override;

    void setTranscodeFilters(nx::core::transcoding::FilterChainPtr filters);
    void setServerTimeZone(QTimeZone value);
    void setTranscoderFixedFrameRate(int value);
    void setTranscoderQuality(Qn::StreamQuality quality);
    void addRecordingContext(const QString& fileName, const QnStorageResourcePtr& storage);
    bool addRecordingContext(const QString& fileName);
    void forceAudioTranscoding() { m_forceAudioTranscoding = true; }
    QString fixedFileName() const;

    // It used to skip some video frames inside GOP when transcoding is used
    void setPreciseStartPosition(int64_t startTimeUs);
    virtual std::optional<nx::recording::Error> getLastError() const override;

protected:
    virtual void onFlush(StorageContext& context) override;
    virtual bool saveData(const QnConstAbstractMediaDataPtr& md) override;
    virtual void setLastError(nx::recording::Error::Code code) override;

    // Overridden to remove gaps from the stream.
    virtual qint64 getPacketTimeUsec(const QnConstAbstractMediaDataPtr& md) override;

private:
    QnAbstractStreamDataProvider* m_mediaProvider;
    bool m_isLayoutsInitialised = false;
    int64_t m_preciseStartTimeUs = 0;
    nx::core::transcoding::FilterChainPtr m_transcodeFilters;
    QTimeZone m_timeZone{QTimeZone::LocalTime};
    AVCodecID m_dstVideoCodec = AV_CODEC_ID_NONE;
    std::unique_ptr<QnFfmpegVideoTranscoder> m_videoTranscoder;
    std::unique_ptr<QnFfmpegAudioTranscoder> m_audioTranscoder;
    Qn::StreamQuality m_transcodeQuality = Qn::StreamQuality::normal;
    int m_transcoderFixedFrameRate = 0;
    AVCodecID m_lastVideoCodec = AV_CODEC_ID_NONE;
    CodecParametersConstPtr m_audioCodecParameters;
    MediaSigner m_signer;
    bool m_stitchTimestampGaps = true;
    TimestampCorrector m_timestampStitcher;
    bool m_forceAudioTranscoding = false;

private:
    virtual void adjustMetaData(QnAviArchiveMetadata& metaData) const override;
    virtual void beforeIoClose(StorageContext& context) override;
    virtual int getStreamIndex(const QnConstAbstractMediaDataPtr& mediaData) const override;
    virtual void writeData(const QnConstAbstractMediaDataPtr& md, int streamIndex) override;
    virtual void onSuccessfulPacketWrite(
        AVCodecParameters* avCodecParams, const uint8_t* data, int size) override;

    virtual CodecParametersPtr getVideoCodecParameters(
        const QnConstCompressedVideoDataPtr& videoData) override;
    virtual CodecParametersConstPtr getAudioCodecParameters(
        const CodecParametersConstPtr& sourceCodecpar, const std::string& container) override;

    virtual bool needToTruncate(const QnConstAbstractMediaDataPtr& md) const override;
    virtual void onSuccessfulWriteData(const QnConstAbstractMediaDataPtr& md) override;
    virtual void onSuccessfulPrepare() override;
    virtual void reportFinished() override;
    virtual void afterClose() override;
    virtual void fileFinished(
        qint64 durationMs, const QString& fileName, qint64 fileSize, qint64 startTimeMs = AV_NOPTS_VALUE) override;

    virtual bool fileStarted(qint64 startTimeMs, int timeZone, const QString& fileName) override;
    virtual void initMetadataStream(StorageContext& context) override;

    bool isTranscodingEnabled() const;
    void updateSignatureAttr(StorageContext* context);
};

} // namespace nx::vms::client::desktop
