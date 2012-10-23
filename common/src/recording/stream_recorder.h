#ifndef _STREAM_RECORDER_H__
#define _STREAM_RECORDER_H__

#include <QBuffer>
#include <QtGui/QPixmap>

#include <utils/common/cryptographic_hash.h>

#include <core/dataconsumer/abstract_data_consumer.h>
#include <core/datapacket/media_data_packet.h>
#include <core/resource/resource.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource/storage_resource.h>

class QnAbstractMediaStreamDataProvider;
class QnFfmpegAudioTranscoder;

class QnStreamRecorder : public QnAbstractDataConsumer
{
    Q_OBJECT

public:
    enum Role {Role_ServerRecording, Role_FileExport, Role_FileExportWithEmptyContext};

    QnStreamRecorder(QnResourcePtr dev);
    virtual ~QnStreamRecorder();

    /*
    * Start new file approx. every N seconds
    */
    void setTruncateInterval(int seconds);

    void setFileName(const QString& fileName);
    
    /*
    * Export motion stream to separate file
    */
    void setMotionFileList(QSharedPointer<QBuffer> motionFileList[CL_MAX_CHANNELS]);

    void close();
    
    qint64 duration() const  { return m_endDateTime - m_startDateTime; }
    
    virtual bool processData(QnAbstractDataPacketPtr data);

    void setStartOffset(qint64 value);

    QnResourcePtr getResource() const { return m_device; }

    QString fixedFileName() const;
    void setEofDateTime(qint64 value);

    /*
    * Calc hash for writing file
    */
    void setNeedCalcSignature(bool value);

    void setSignLogo(QPixmap logo);

    /*
    * Return hash value 
    */
    QByteArray getSignature() const;

    void setRole(Role role);

    void setStorage(QnStorageResourcePtr storage);

    void setContainer(const QString& container);
    void setNeedReopen();
    bool isAudioPresent() const;

    /*
    * Transcode to specified audio codec is source codec is different
    */
    void setAudioCodec(CodecID codec);
signals:
    void recordingFailed(QString errMessage);
    void recordingStarted();

    void recordingFinished(QString fileName);
    void recordingProgress(int progress);
protected:
    virtual void endOfRun();
    bool initFfmpegContainer(QnCompressedVideoDataPtr mediaData);

    void setPrebufferingUsec(int value);
    void flushPrebuffer();
    int getPrebufferingUsec() const;
    virtual bool needSaveData(QnAbstractMediaDataPtr media);

    virtual bool saveMotion(QnMetaDataV1Ptr media);

    virtual void fileFinished(qint64 durationMs, const QString& fileName, QnAbstractMediaStreamDataProvider *provider, qint64 fileSize) {
        Q_UNUSED(durationMs) Q_UNUSED(fileName) Q_UNUSED(provider) Q_UNUSED(fileSize)
    }
    virtual void fileStarted(qint64 startTimeMs, const QString& fileName, QnAbstractMediaStreamDataProvider *provider) {
        Q_UNUSED(startTimeMs) Q_UNUSED(fileName) Q_UNUSED(provider)
    }
    virtual QString fillFileName(QnAbstractMediaStreamDataProvider*);

    bool addSignatureFrame(QString& errorString);
private:
    bool saveData(QnAbstractMediaDataPtr md);
private:
    void markNeedKeyData();
    void writeData(QnAbstractMediaDataPtr md, int streamIndex);
    void updateSignatureAttr();
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
private:
    bool m_waitEOF;

    bool m_forceDefaultCtx;
    AVFormatContext* m_formatCtx;
    bool m_packetWrited;
    QString m_lastErrMessage;
    qint64 m_currentChunkLen;

    QString m_fileName;
    qint64 m_startOffset;
    int m_prebufferingUsec;
    QnUnsafeQueue<QnAbstractMediaDataPtr> m_prebuffer;

    qint64 m_EofDateTime;
    bool m_endOfData;
    int m_lastProgress;
    bool m_needCalcSignature;
    QnAbstractMediaStreamDataProvider* m_mediaProvider;
    
    Role m_role;
    QnCryptographicHash m_mdctx;
    QPixmap m_logo;
    QString m_container;
    int m_videoChannels;
    QnCodecAudioFormat m_prevAudioFormat;
    AVIOContext* m_ioContext;
    bool m_needReopen;
    bool m_isAudioPresent;
    QnCompressedVideoDataPtr m_lastIFrame;
    QSharedPointer<QIODevice> m_motionFileList[CL_MAX_CHANNELS];
    QnFfmpegAudioTranscoder* m_audioTranscoder;
    CodecID m_dstAudioCodec;
};

#endif // _STREAM_RECORDER_H__
