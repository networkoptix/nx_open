#ifndef _STREAM_RECORDER_H__
#define _STREAM_RECORDER_H__

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QBuffer>
#include <QtGui/QImage>

extern "C"
{
    #include <libavformat/avformat.h>
}

#include <utils/common/cryptographic_hash.h>

#include <core/ptz/item_dewarping_params.h>

#include <core/dataconsumer/abstract_data_consumer.h>
#include <core/datapacket/audio_data_packet.h>
#include <core/datapacket/video_data_packet.h>
#include <core/resource/resource.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource/storage_resource.h>
#include "utils/color_space/image_correction.h"

class QnAbstractMediaStreamDataProvider;
class QnFfmpegAudioTranscoder;
class QnFfmpegVideoTranscoder;

class QnStreamRecorder : public QnAbstractDataConsumer
{
    Q_OBJECT
    Q_ENUMS(StreamRecorderError)
    Q_ENUMS(Role)
public:
    // TODO: #Elric #enum
    enum Role {Role_ServerRecording, Role_FileExport, Role_FileExportWithEmptyContext};

    enum StreamRecorderError {
        NoError = 0,
        ContainerNotFoundError,
        FileCreateError,
        VideoStreamAllocationError,
        AudioStreamAllocationError,
        InvalidAudioCodecError,
        IncompatibleCodecError,

        LastError
    };

    static QString errorString(int errCode);

    QnStreamRecorder(const QnResourcePtr& dev);
    virtual ~QnStreamRecorder();

    /*
    * Start new file approx. every N seconds
    */
    void setTruncateInterval(int seconds);

    void setFileName(const QString& fileName);
    QString getFileName() const;
    
    /*
    * Export motion stream to separate file
    */
    void setMotionFileList(QSharedPointer<QBuffer> motionFileList[CL_MAX_CHANNELS]);

    void close();
    
    qint64 duration() const  { return m_endDateTime - m_startDateTime; }
    
    virtual bool processData(const QnAbstractDataPacketPtr& data) override;

    void setStartOffset(qint64 value);

    QnResourcePtr getResource() const { return m_device; }

    QString fixedFileName() const;
    void setEofDateTime(qint64 value);

    /*
    * Calc hash for writing file
    */
    void setNeedCalcSignature(bool value);

#ifdef SIGN_FRAME_ENABLED
    void setSignLogo(const QImage& logo);
#endif

    /*
    * Return hash value 
    */
    QByteArray getSignature() const;

    void setRole(Role role);
    void setTimestampCorner(Qn::Corner pos);

    void setStorage(const QnStorageResourcePtr& storage);

    void setContainer(const QString& container);
    void setNeedReopen();
    bool isAudioPresent() const;

    /*
    * Transcode to specified audio codec is source codec is different
    */
    void setAudioCodec(CodecID codec);

    /*
    * Time difference between client and server time zone. Used for onScreen timestamp drawing
    */
    void setOnScreenDateOffset(qint64 timeOffsetMs);

    void setContrastParams(const ImageCorrectionParams& params);

    void setItemDewarpingParams(const QnItemDewarpingParams& params);

    /*
    * Server time zone. Used for export to avi/mkv files
    */
    void setServerTimeZoneMs(qint64 value);

    void setSrcRect(const QRectF& srcRect);
signals:
    void recordingStarted();
    void recordingProgress(int progress);
    void recordingFinished(int status, const QString &fileName);
protected:
    virtual void endOfRun();
    bool initFfmpegContainer(const QnConstCompressedVideoDataPtr& mediaData);

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
    virtual QString fillFileName(QnAbstractMediaStreamDataProvider*);

    bool addSignatureFrame();
    void markNeedKeyData();
    virtual bool saveData(const QnConstAbstractMediaDataPtr& md);
    virtual void writeData(const QnConstAbstractMediaDataPtr& md, int streamIndex);
private:
    void updateSignatureAttr();
    qint64 findNextIFrame(qint64 baseTime);
protected:
    QnResourcePtr m_device;
    bool m_firstTime;
    bool m_gotKeyFrame[CL_MAX_CHANNELS];
    qint64 m_truncateInterval;
    QString m_fixedFileName;
    qint64 m_endDateTime;
    qint64 m_startDateTime;
    bool m_stopOnWriteError;
    QnStorageResourcePtr m_storage;
    int m_currentTimeZone;
private:
    bool m_waitEOF;

    bool m_forceDefaultCtx;
    AVFormatContext* m_formatCtx;
    bool m_packetWrited;
    int  m_lastError;
    qint64 m_currentChunkLen;

    QString m_fileName;
    qint64 m_startOffset;
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
    int m_videoChannels;
    QnCodecAudioFormat m_prevAudioFormat;
    AVIOContext* m_ioContext;
    bool m_needReopen;
    bool m_isAudioPresent;
    QnConstCompressedVideoDataPtr m_lastIFrame;
    QSharedPointer<QIODevice> m_motionFileList[CL_MAX_CHANNELS];
    QnFfmpegAudioTranscoder* m_audioTranscoder;
    QnFfmpegVideoTranscoder* m_videoTranscoder;
    CodecID m_dstAudioCodec;
    CodecID m_dstVideoCodec;
    qint64 m_onscreenDateOffset;
    Qn::Corner m_timestampCorner;
    qint64 m_serverTimeZoneMs;

    qint64 m_nextIFrameTime;
    qint64 m_truncateIntervalEps;
    QRectF m_srcRect;
    ImageCorrectionParams m_contrastParams;
    QnItemDewarpingParams m_itemDewarpingParams;

    /** If true method close() will emit signal recordingFinished() at the end. */
    bool m_recordingFinished;
    Role m_role;
};

#endif // ENABLE_DATA_PROVIDERS

#endif // _STREAM_RECORDER_H__
