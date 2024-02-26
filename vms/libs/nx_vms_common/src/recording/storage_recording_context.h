// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <recording/abstract_recording_context.h>
#include <recording/abstract_recording_context_callback.h>
#include <recording/stream_recorder_data.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/avi/avi_archive_metadata.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <utils/media/ffmpeg_helper.h>
#include <nx/streaming/video_data_packet.h>
#include <utils/media/annexb_to_mp4.h>

extern "C"
{
#include <libavformat/avformat.h>
}

#include <QtCore/QString>

namespace nx {

/**
 * Storage-based implementation of AbstractRecordingContext policy. Creates a media file on
 * storage(s) and writes media packets to this file.
 */
class NX_VMS_COMMON_API StorageRecordingContext:
    public virtual AbstractRecordingContext, //< Implements.
    // Public, because private breaks clang build.
    public virtual /*mix-in*/ AbstractRecordingContextCallback //< Uses.
{
protected:
    struct StorageContext
    {
        QString fileName;
        AVFormatContext* formatCtx = nullptr;
        QnStorageResourcePtr storage;
        qint64 totalWriteTimeNs = 0;
        QnAviArchiveMetadata metadata;
        QnAviArchiveMetadata::Format fileFormat = QnAviArchiveMetadata::Format::custom;
        AVStream* metadataStream = nullptr;

        StorageContext(const QString& fileName, const QnStorageResourcePtr& storage):
            fileName(fileName), storage(storage)
        {
        }

        StorageContext() = default;
        bool isEmpty() const { return formatCtx == nullptr; }
    };

public:
    StorageRecordingContext(bool exportMode = false);
    void setContainer(const QString& container);
    void setLastError(nx::recording::Error::Code code);

    // AbstractRecordingContext.
    virtual void closeRecordingContext(std::chrono::milliseconds durationMs) override;
    virtual void writeData(const QnConstAbstractMediaDataPtr& md, int streamIndex) override;
    virtual AVMediaType streamAvMediaType(
        QnAbstractMediaData::DataType dataType, int streamIndex) const override;
    virtual int streamCount() const override;
    virtual bool doPrepareToStart(
        const QnConstAbstractMediaDataPtr& mediaData,
        const QnConstResourceVideoLayoutPtr& videoLayout,
        const AudioLayoutConstPtr& audioLayout,
        const QnAviArchiveMetadata& metadata) override;
    virtual std::optional<nx::recording::Error> getLastError() const override;

protected:
    StorageContext m_recordingContext;

    virtual CodecParametersPtr getVideoCodecParameters(
        const QnConstCompressedVideoDataPtr& videoData);
    virtual CodecParametersConstPtr getAudioCodecParameters(
        const CodecParametersConstPtr& sourceCodecParams, const QString& container);

    virtual void initMetadataStream(StorageContext& context) = 0;
    virtual void initIoContext(StorageContext& context);
    virtual void initializeRecordingContext(int64_t startTimeUs);
    virtual void onFlush(StorageContext& /*context*/) {};
    virtual void beforeIoClose(StorageContext& context) = 0;
    virtual void onSuccessfulPacketWrite(
        AVCodecParameters* avCodecParams, const uint8_t* data, int size) = 0;

    virtual void fileFinished(
        qint64 durationMs, const QString& fileName, qint64 fileSize, qint64 startTimeMs) = 0;

    virtual bool fileStarted(qint64 startTimeMs, int timeZone, const QString& fileName) = 0;

private:
    const bool m_exportMode;
    QString m_container = "matroska";
    bool m_packetWritten = false;
    std::optional<nx::recording::Error> m_lastError;
    nx::media::AnnexbToMp4 m_annexbToMp4;

    virtual qint64 getPacketTimeUsec(const QnConstAbstractMediaDataPtr& md);

    void cleanFfmpegContexts();
    void writeHeader(StorageContext& context);
    void allocateFfmpegObjects(
        const QnConstAbstractMediaDataPtr& mediaData,
        const QnConstResourceVideoLayoutPtr& videoLayout,
        const AudioLayoutConstPtr& audioLayout,
        StorageContext& context);
};

} // namespace nx
