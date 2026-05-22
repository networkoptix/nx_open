// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QTimeZone>

#include <export/signer.h>
#include <recording/storage_recording_context.h>
#include <recording/stream_recorder.h>

namespace nx::vms::client::desktop {


/*
 * If the codec changes, the export will start in a new file. The list of
 * files is stored as timestamps of the beginning of a new file. The metadata
 * and watermark are stored in the first file.
 */
class NovMediaExport:
    public QnStreamRecorder,
    public nx::StorageRecordingContext
{
public:
    NovMediaExport(const QnMediaResourcePtr& resource,
        QnAbstractStreamDataProvider* mediaProvider,
        const QnStorageResourcePtr& storage,
        const QString& filename,
        int64_t startTimeUs,
        int64_t endTimeUs);
    virtual ~NovMediaExport() override;
    virtual std::optional<nx::recording::Error> getLastError() const override;

private:
    virtual bool saveData(const QnConstAbstractMediaDataPtr& md) override;
    virtual void setLastError(nx::recording::Error::Code code) override;
    virtual void initializeRecordingContext(int64_t startTimeUs) override;
    virtual void endOfRun() override;
    virtual void adjustMetaData(QnAviArchiveMetadata& metaData) const override;
    virtual void onSuccessfulPacketWrite(
        AVCodecParameters* avCodecParams, const uint8_t* data, int size) override;
    virtual void reportFinished() override;

    // Empty callbacks.
    virtual void onSuccessfulWriteData(const QnConstAbstractMediaDataPtr& /*data*/) override {};
    virtual void beforeIoClose(StorageContext& /*context*/) override {};
    virtual void initMetadataStream(StorageContext& /*context*/) override {};
    virtual void fileFinished(
        qint64 /*durationMs*/, const QString& /*fileName*/, qint64 /*fileSize*/, qint64 /*startTimeMs*/) override {};
    virtual bool fileStarted(
        qint64 /*startTimeMs*/, int /*timeZone*/, const QString& /*fileName*/) override  { return true; };

    bool saveMotion(const QnConstMetaDataV1Ptr& media) override;
    bool writeFile(const QString& fileName, const QByteArray& data);
    void writeMotionData();

private:
    QString m_baseFileName;
    QnStorageResourcePtr m_storage;
    QnAbstractStreamDataProvider* m_mediaProvider;
    QTimeZone m_timeZone{QTimeZone::LocalTime};
    CodecParametersConstPtr m_videoCodecParameters;
    CodecParametersConstPtr m_audioCodecParameters;
    MediaSigner m_signer;
    QString m_metadataFileName;
    std::vector<int64_t> m_startTimestamps;
    bool m_finishAllFiles = false;
    std::unique_ptr<QBuffer> m_motionFileList[CL_MAX_CHANNELS];
};

} // namespace nx::vms::client::desktop
