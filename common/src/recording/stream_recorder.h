#ifndef _STREAM_RECORDER_H__
#define _STREAM_RECORDER_H__

#include "core/dataconsumer/dataconsumer.h"
#include "core/datapacket/mediadatapacket.h"
#include "core/resource/resource.h"
#include "core/resource/resource_media_layout.h"
#include <openssl/evp.h>
#include <QPixmap>

#include "core/resource/storage_resource.h"

class QnAbstractMediaStreamDataProvider;

class QnStreamRecorder : public QnAbstractDataConsumer
{
    Q_OBJECT

public:
    enum Role {Role_ServerRecording, Role_FileExport};

    QnStreamRecorder(QnResourcePtr dev);
    virtual ~QnStreamRecorder();

    /*
    * Start new file approx. every N seconds
    */
    void setTruncateInterval(int seconds);

    void setFileName(const QString& fileName);
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

    virtual bool saveMotion(QnAbstractMediaDataPtr media);

    virtual void fileFinished(qint64 /*durationMs*/, const QString& /*fileName*/, QnAbstractMediaStreamDataProvider*) {}
    virtual void fileStarted(qint64 /*startTimeMs*/, const QString& /*fileName*/, QnAbstractMediaStreamDataProvider*) {}
    virtual QString fillFileName(QnAbstractMediaStreamDataProvider*);

    bool addSignatureFrame(QString& errorString);
private:
    bool saveData(QnAbstractMediaDataPtr md);
private:
    void markNeedKeyData();
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
    QQueue<QnAbstractMediaDataPtr> m_prebuffer;

    qint64 m_EofDateTime;
    bool m_endOfData;
    int m_lastProgress;
    bool m_needCalcSignature;
    QnAbstractMediaStreamDataProvider* m_mediaProvider;
    
    Role m_role;
    EVP_MD_CTX* m_mdctx;
    QPixmap m_logo;
    QString m_container;
    int m_videoChannels;
    QnCodecAudioFormat m_prevAudioFormat;
};

#endif // _STREAM_RECORDER_H__
