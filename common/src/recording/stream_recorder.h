#ifndef _STREAM_RECORDER_H__
#define _STREAM_RECORDER_H__

#include "core/resource/resource.h"
#include "core/resource/resource_media_layout.h"
#include "core/datapacket/mediadatapacket.h"

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
    qint64 duration() const { return m_lastPacketTime; }
    virtual bool processData(QnAbstractDataPacketPtr data);
signals:
    void recordingFailed(QString errMessage);
    void recordingStarted();
protected:
	virtual void endOfRun();
    bool initFfmpegContainer(QnCompressedVideoDataPtr mediaData);
protected:
	QnResourcePtr m_device;
	bool m_firstTime;
	bool m_gotKeyFrame[CL_MAX_CHANNELS];
private:
    AVFormatContext* m_formatCtx;
    bool m_packetWrited;
    qint64 m_firstTimestamp;
    QString m_lastErrMessage;
    qint64 m_truncateInterval;
    qint64 m_currentChunkLen;
    
    //QDateTime m_currentDateTime;
    //QDateTime m_prevDateTime;
    qint64 m_currentDateTime;
    qint64 m_prevDateTime;
    QString m_fileName;
    QString m_fixedFileName;
    qint64 m_lastPacketTime;
};

#endif //_STREAM_RECORDER_H__
