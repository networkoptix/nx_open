#ifndef _STREAM_RECORDER_H__
#define _STREAM_RECORDER_H__

#include "core/dataconsumer/dataconsumer.h"
#include "core/datapacket/mediadatapacket.h"
#include "core/resource/resource.h"
#include "core/resource/resource_media_layout.h"

class QnStreamRecorder : public QnAbstractDataConsumer
{
    Q_OBJECT

public:
    QnStreamRecorder(QnResourcePtr dev);
    virtual ~QnStreamRecorder();

    /*
    * Start new file approx. every N seconds
    */
    void setTruncateInterval(int seconds);

    void setFileName(const QString& fileName);
    void close();
    
    qint64 duration() const  { return m_lastPacketTime - m_firstTimestamp; }
    
    virtual bool processData(QnAbstractDataPacketPtr data);

    void setStartOffset(qint64 value);

Q_SIGNALS:
    void recordingFailed(QString errMessage);
    void recordingStarted();

protected:
    virtual void endOfRun();
    bool initFfmpegContainer(QnCompressedVideoDataPtr mediaData);

    void setPrebufferingUsec(int value);
    void flushPrebuffer();
    int getPrebufferingUsec() const;
    virtual bool needSaveData(QnAbstractMediaDataPtr media);

    virtual bool saveMotion(QnAbstractMediaDataPtr media);
    bool saveData(QnAbstractMediaDataPtr md);

    virtual void fileFinished(qint64 /*durationMs*/, const QString& /*fileName*/) {}
    virtual void fileStarted(qint64 /*startTimeMs*/, const QString& /*fileName*/) {}
    virtual QString fillFileName();
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
private:
    bool m_forceDefaultCtx;
    AVFormatContext* m_formatCtx;
    bool m_packetWrited;
    qint64 m_firstTimestamp;
    QString m_lastErrMessage;
    qint64 m_currentChunkLen;

    QString m_fileName;
    qint64 m_lastPacketTime;
    qint64 m_startOffset;
    int m_prebufferingUsec;
    QQueue<QnAbstractMediaDataPtr> m_prebuffer;
};

#endif // _STREAM_RECORDER_H__
