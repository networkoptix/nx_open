#ifndef _STREAM_RECORDER_H__
#define _STREAM_RECORDER_H__

#ifdef ENABLE_DATA_PROVIDERS

#include <random>

#include <QtCore/QBuffer>
#include <QtGui/QImage>

#include <boost/optional/optional.hpp>

extern "C"
{
    #include <libavformat/avformat.h>
}

#include <utils/common/cryptographic_hash.h>

#include <core/ptz/item_dewarping_params.h>

#include <nx/streaming/abstract_data_consumer.h>
#include <nx/streaming/audio_data_packet.h>
#include <nx/streaming/video_data_packet.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource_media_layout.h>

#include <utils/color_space/image_correction.h>
#include <core/resource/resource_consumer.h>
#include <transcoding/filters/filter_helper.h>

#include <recording/stream_recorder_data.h>
#include <boost/optional.hpp>
#include <common/common_module_aware.h>

class QnAbstractMediaStreamDataProvider;
class QnFfmpegAudioTranscoder;
class QnFfmpegVideoTranscoder;

class QnStreamRecorder:
    public QnAbstractDataConsumer,
    public QnResourceConsumer,
    public QnCommonModuleAware
{
    Q_OBJECT

public:
    static QString errorString(StreamRecorderError errCode);

    QnStreamRecorder(QnCommonModule* commonModule, const QnResourcePtr& dev);
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

    void close();

    qint64 duration() const  { return m_endDateTime - m_startDateTime; }

    virtual bool processData(const QnAbstractDataPacketPtr& data) override;

    QnResourcePtr getResource() const { return m_device; }

    QString fixedFileName() const;
    void setEofDateTime(qint64 value);

    /*
    * Calc hash for writing file
    */
    void setNeedCalcSignature(bool value);

    virtual void disconnectFromResource() override;
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

    void setExtraTranscodeParams(const QnImageFilterHelper& extraParams);

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

    virtual void fileFinished(qint64 durationMs, const QString& fileName, QnAbstractMediaStreamDataProvider *provider, qint64 fileSize) {
        Q_UNUSED(durationMs) Q_UNUSED(fileName) Q_UNUSED(provider) Q_UNUSED(fileSize)
    }
    virtual void fileStarted(qint64 startTimeMs, int timeZone, const QString& fileName, QnAbstractMediaStreamDataProvider *provider) {
        Q_UNUSED(startTimeMs) Q_UNUSED(timeZone) Q_UNUSED(fileName) Q_UNUSED(provider)
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

private:
    void updateSignatureAttr(size_t i);
    qint64 findNextIFrame(qint64 baseTime);
    void cleanFfmpegContexts();
protected:
    QnResourcePtr m_device;
    bool m_firstTime;
    bool m_gotKeyFrame[CL_MAX_CHANNELS];
    qint64 m_truncateInterval;
    bool m_fixedFileName;
    qint64 m_endDateTime;
    qint64 m_startDateTime;
    int m_currentTimeZone;
    std::vector<StreamRecorderContext> m_recordingContextVector;

private:
    bool m_waitEOF;

    bool m_forceDefaultCtx;
    bool m_packetWrited;
    StreamRecorderErrorStruct m_lastError;
    qint64 m_currentChunkLen;

    int m_prebufferingUsec;
    QnUnsafeQueue<QnConstAbstractMediaDataPtr> m_prebuffer;

    qint64 m_EofDateTime;
    bool m_endOfData;
    int m_lastProgress;
    bool m_needCalcSignature;
    QnAbstractMediaStreamDataProvider* m_mediaProvider;

    QnCryptographicHash m_mdctx;
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
    QnImageFilterHelper m_extraTranscodeParams;

    std::random_device m_rd;
    std::mt19937 m_gen;
};

#endif // ENABLE_DATA_PROVIDERS

#endif // _STREAM_RECORDER_H__
