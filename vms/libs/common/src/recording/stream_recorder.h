#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <random>
#include <chrono>

#include <QtCore/QBuffer>
#include <QtGui/QImage>

#include <boost/optional/optional.hpp>

extern "C"
{
    #include <libavformat/avformat.h>
}

#include <nx/utils/cryptographic_hash.h>

#include <nx/streaming/abstract_data_consumer.h>
#include <nx/streaming/audio_data_packet.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/utils/random_qt_device.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource_media_layout.h>

#include <utils/color_space/image_correction.h>
#include <core/resource/resource_consumer.h>

#include <nx/core/transcoding/filters/filter_chain.h>

#include <recording/stream_recorder_data.h>

#include <common/common_module_aware.h>
#include <core/resource/avi/avi_archive_metadata.h>

class QnAbstractMediaStreamDataProvider;
class QnFfmpegAudioTranscoder;
class QnFfmpegVideoTranscoder;

class QnStreamRecorder:
    public QnAbstractDataConsumer,
    public QnResourceConsumer,
    public /*mixin*/ QnCommonModuleAware
{
    Q_OBJECT

public:
    static QString errorString(StreamRecorderError errCode);

    QnStreamRecorder(const QnResourcePtr& dev);
    virtual ~QnStreamRecorder();

    /*
    * Start new file approx. every N seconds
    */
    void setTruncateInterval(int seconds);

    void addRecordingContext(
        const QString               &fileName,
        const QnStorageResourcePtr  &storage
    );

    // Sets default file name and tries to get relevant
    // storage.
    bool addRecordingContext(const QString &fileName);

    /*
    * Export motion stream to separate file
    */
    void setMotionFileList(QSharedPointer<QBuffer> motionFileList[CL_MAX_CHANNELS]);

    // It used to skip some video frames inside GOP when transcoding is used
    void setPreciseStartPosition(int64_t startTimeUs);

    void close();

    qint64 duration() const  { return m_endDateTimeUs - m_startDateTimeUs; }

    virtual bool processData(const QnAbstractDataPacketPtr& data) override;

    QString fixedFileName() const;
    void setProgressBounds(qint64 bof, qint64 eof);

    /*
    * Calc hash for writing file
    */
    void setNeedCalcSignature(bool value);

    virtual void disconnectFromResource() override;

    void forceAudioLayout(const QnResourceAudioLayoutPtr& layout);

    void disableRegisterFile(bool disable);

#ifdef SIGN_FRAME_ENABLED
    void setSignLogo(const QImage& logo);
#endif

    /*
    * Return hash value
    */
    QByteArray getSignature() const;

    void setRole(StreamRecorderRole role);

    void setContainer(const QString& container);
    void setNeedReopen();
    bool isAudioPresent() const;

    /*
    * Transcode to specified audio codec is source codec is different
    */
    void setAudioCodec(AVCodecID codec);

    /*
    * Server time zone. Used for export to avi/mkv files
    */
    void setServerTimeZoneMs(qint64 value);

    void setTranscodeFilters(const nx::core::transcoding::FilterChain& filters);

    int64_t lastFileSize() const;

signals:
    void recordingStarted();
    void recordingProgress(int progress);
    void recordingFinished(const StreamRecorderErrorStruct &status, const QString &fileName);
protected:
    virtual void endOfRun();
    bool initFfmpegContainer(const QnConstAbstractMediaDataPtr& mediaData);

    void setPrebufferingUsec(int value);
    void flushPrebuffer();
    int getPrebufferingUsec() const;
    virtual bool needSaveData(const QnConstAbstractMediaDataPtr& media);

    virtual bool saveMotion(const QnConstMetaDataV1Ptr& media);

    virtual void fileFinished(
        qint64 /*durationMs*/,
        const QString& /*fileName*/,
        QnAbstractMediaStreamDataProvider* /*provider*/,
        qint64 /*fileSize*/,
        qint64 startTimeMs = AV_NOPTS_VALUE)
    {
        Q_UNUSED(startTimeMs)
    }
    virtual void fileStarted(
        qint64 /*startTimeMs*/,
        int /*timeZone*/,
        const QString& /*fileName*/,
        QnAbstractMediaStreamDataProvider* /*provider*/,
        bool sideRecorder = false)
    {
        Q_UNUSED(sideRecorder)
    }
    virtual void getStoragesAndFileNames(QnAbstractMediaStreamDataProvider*);

    bool addSignatureFrame();
    void markNeedKeyData();
    virtual bool saveData(const QnConstAbstractMediaDataPtr& md);
    virtual void writeData(const QnConstAbstractMediaDataPtr& md, int streamIndex);
    virtual void initIoContext(
        const QnStorageResourcePtr& storage,
        const QString& url,
        AVIOContext** context);
    virtual qint64 getPacketTimeUsec(const QnConstAbstractMediaDataPtr& md);
    virtual bool isUtcOffsetAllowed() const { return true; }
    virtual void updateContainerMetadata(QnAviArchiveMetadata* /*metadata*/) const {}
    virtual bool forceDefaultContext(const QnConstAbstractMediaDataPtr& mediaData) const;

    void setTranscoderQuality(Qn::StreamQuality quality);
    void setTranscoderFixedFrameRate(int value);
private:
    struct StreamRecorderContext
    {
        QString fileName;
        AVFormatContext* formatCtx = nullptr;
        QnStorageResourcePtr storage;
        qint64 totalWriteTimeNs = 0;
        QnAviArchiveMetadata metadata;
        QnAviArchiveMetadata::Format fileFormat = QnAviArchiveMetadata::Format::custom;
        AVStream* metadataStream = nullptr;
        StreamRecorderContext(const QString& fileName, const QnStorageResourcePtr& storage);
    };

    /**
     * It is impossible to write avi/mkv attribute in the end, so write magic on startup, then
     * update it.
     */
    void updateSignatureAttr(StreamRecorderContext* context);
    qint64 findNextIFrame(qint64 baseTime);
    void cleanFfmpegContexts();
    void addSignatureFrameIfNeed();
    void updateProgress(qint64 timestampUs);
    void flushTranscoder();
protected:
    bool m_firstTime;
    bool m_gotKeyFrame[CL_MAX_CHANNELS];
    qint64 m_truncateInterval;
    bool m_fixedFileName;
    qint64 m_endDateTimeUs;
    qint64 m_startDateTimeUs;
    int m_currentTimeZone;
    std::vector<StreamRecorderContext> m_recordingContextVector;

private:
    int64_t m_preciseStartTimeUs = 0;
    bool m_waitEOF;

    bool m_packetWrited;
    StreamRecorderErrorStruct m_lastError;
    qint64 m_currentChunkLen;

    int m_prebufferingUsec;
    QnUnsafeQueue<QnConstAbstractMediaDataPtr> m_prebuffer;

    qint64 m_bofDateTimeUs;
    qint64 m_eofDateTimeUs;
    bool m_endOfData;
    int m_lastProgress;
    bool m_needCalcSignature;
    QnAbstractMediaStreamDataProvider* m_mediaProvider;

    nx::utils::QnCryptographicHash m_mdctx;
#ifdef SIGN_FRAME_ENABLED
    QImage m_logo;
#endif
    QString m_container;
    boost::optional<QnCodecAudioFormat> m_prevAudioFormat;
    bool m_needReopen;
    bool m_isAudioPresent;
    QnConstCompressedVideoDataPtr m_lastIFrame;
    QSharedPointer<QIODevice> m_motionFileList[CL_MAX_CHANNELS];
    QnFfmpegAudioTranscoder* m_audioTranscoder;
    QnFfmpegVideoTranscoder* m_videoTranscoder;
    AVCodecID m_dstAudioCodec;
    AVCodecID m_dstVideoCodec;
    qint64 m_serverTimeZoneMs;

    qint64 m_nextIFrameTime;
    qint64 m_truncateIntervalEps;

    /** If true method close() will emit signal recordingFinished() at the end. */
    bool m_recordingFinished;
    StreamRecorderRole m_role;
    boost::optional<nx::core::transcoding::FilterChain> m_transcodeFilters;

    nx::utils::random::QtDevice m_rd;
    std::mt19937 m_gen;

    QnResourceAudioLayoutPtr m_forcedAudioLayout;
    bool m_disableRegisterFile;
    int64_t m_lastFileSize = 0;
    bool m_interleavedStream = false;
    Qn::StreamQuality m_transcodeQuality = Qn::StreamQuality::normal;
    int m_transcoderFixedFrameRate = 0;
    mutable QnMutex m_mutex;
    AVCodecID m_lastCompressionType = AV_CODEC_ID_NONE;
};

#endif // ENABLE_DATA_PROVIDERS
