// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>
#include <random>

#include <QtCore/QBuffer>
#include <QtGui/QImage>

#include <analytics/common/object_metadata.h>
#include <core/resource/avi/avi_archive_metadata.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/resource_media_layout.h>
#include <nx/media/audio_data_packet.h>
#include <nx/media/video_data_packet.h>
#include <nx/streaming/abstract_data_consumer.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/utils/cryptographic_hash.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <recording/abstract_recording_context.h>
#include <recording/abstract_recording_context_callback.h>
#include <recording/stream_recorder_data.h>
#include <utils/color_space/image_correction.h>

namespace nx::common::metadata { class QnCompressedObjectMetadataPacket; }

class NX_VMS_COMMON_API QnStreamRecorder:
    public QnAbstractDataConsumer,
    // Public, because private breaks clang build.
    public virtual /*mix-in*/ nx::AbstractRecordingContext, //< Uses.
    public virtual nx::AbstractRecordingContextCallback //< Implements.
{
    Q_OBJECT

public:

    QnStreamRecorder(const QnResourcePtr& dev);
    virtual ~QnStreamRecorder();

    /*
    * Export motion stream to separate file
    */
    void setMotionFileList(QSharedPointer<QBuffer> motionFileList[CL_MAX_CHANNELS]);
    void close();
    qint64 duration() const  { return m_endDateTimeUs - m_startDateTimeUs; }
    void setProgressBounds(qint64 bof, qint64 eof);
    void setNeedReopen();
    bool isAudioPresent() const;

    // For tests.
    int64_t packetCount() const;
    void resetPacketCount();

    virtual bool processData(const QnAbstractDataPacketPtr& data) override;

signals:
    void recordingStarted();
    void recordingProgress(int progress);
    void recordingFinished(const std::optional<nx::recording::Error>& status, const QString &fileName);

protected:
    virtual void endOfRun() override;

    void setPrebufferingUsec(int value);
    void markNeedKeyData();
    int getPrebufferingUsec() const;
    void setPreciseStartDateTime(int64_t startTimeUs);

    AudioLayoutConstPtr getAudioLayout();
    QnConstResourceVideoLayoutPtr getVideoLayout();

    void setVideoLayout(const QnConstResourceVideoLayoutPtr& videoLayout);
    void setAudioLayout(const AudioLayoutConstPtr& audioLayout);
    void setHasVideo(bool hasVideo);

    virtual bool needSaveData(const QnConstAbstractMediaDataPtr& media);
    virtual bool saveMotion(const QnConstMetaDataV1Ptr& media);
    virtual bool saveAnalyticsMetadata(
        const nx::common::metadata::ConstObjectMetadataPacketPtr& packet);

    virtual bool saveData(const QnConstAbstractMediaDataPtr& md);
    virtual bool needToTruncate(const QnConstAbstractMediaDataPtr& md) const = 0;
    virtual void onSuccessfulWriteData(const QnConstAbstractMediaDataPtr& md) = 0;
    virtual void onSuccessfulPrepare() = 0;
    virtual void reportFinished();
    virtual void afterClose() = 0;
    virtual bool dataHoleDetected(const QnConstAbstractMediaDataPtr& md);
    virtual void updateContainerMetadata(QnAviArchiveMetadata* /*metadata*/) const {}
    virtual int getStreamIndex(const QnConstAbstractMediaDataPtr& mediaData) const;
    virtual void adjustMetaData(QnAviArchiveMetadata& metaData) const = 0;
    virtual std::vector<uint8_t> prepareEncryptor(quint64 /*nonce*/) { return std::vector<uint8_t>(); }
    virtual QnConstAbstractMediaDataPtr encryptDataIfNeed(const QnConstAbstractMediaDataPtr& data);
    virtual void setLastError(nx::recording::Error::Code code) = 0;

private:
    void flushPrebuffer();
    qint64 isPrimaryStream(const QnConstAbstractMediaDataPtr& md) const;
    qint64 isPrimaryKeyFrame(const QnConstAbstractMediaDataPtr& md) const;
    qint64 findNextIFrame(qint64 baseTime);
    void updateProgress(qint64 timestampUs);
    bool prepareToStart(const QnConstAbstractMediaDataPtr& mediaData);

    QnAviArchiveMetadata prepareMetaData(
        const QnConstAbstractMediaDataPtr& mediaData,
        const QnConstResourceVideoLayoutPtr& videoLayout);

protected:
    // nx::AbstractRecordingContextCallback implementation
    virtual int64_t startTimeUs() const override;
    virtual bool isInterleavedStream() const override;
    virtual bool isUtcOffsetAllowed() const override { return true; }

protected:
    bool m_firstTime = true;
    bool m_gotKeyFrame[CL_MAX_CHANNELS];
    bool m_isAudioPresent = false;
    QnResourcePtr m_resource;
    const QnMediaResourcePtr m_mediaDevice;
    bool m_fileOpened = false;
    bool m_finishReported = false;
    int64_t m_endDateTimeUs = AV_NOPTS_VALUE;
    int64_t m_startDateTimeUs = AV_NOPTS_VALUE;
    bool m_interleavedStream = false;
    bool m_hasVideo = true;
    QnUnsafeQueue<QnConstAbstractMediaDataPtr> m_prebuffer;

private:
    QnConstResourceVideoLayoutPtr m_videoLayout;
    AudioLayoutConstPtr m_audioLayout;
    qint64 m_currentChunkLen = 0;
    int m_prebufferingUsec = 0;
    qint64 m_bofDateTimeUs = AV_NOPTS_VALUE;
    qint64 m_eofDateTimeUs = AV_NOPTS_VALUE;
    int m_lastProgress = -1;
    bool m_needReopen = false;
    QSharedPointer<QIODevice> m_motionFileList[CL_MAX_CHANNELS];
    qint64 m_lastPrimaryTime = AV_NOPTS_VALUE;
    qint64 m_nextIFrameTime = AV_NOPTS_VALUE;
    mutable nx::Mutex m_mutex;
    std::atomic<int64_t> m_packetCount = 0;
};
