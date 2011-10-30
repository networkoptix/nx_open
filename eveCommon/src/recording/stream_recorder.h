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
signals:
    void recordingFailed(QString errMessage);
    void recordingStarted();
protected:
	virtual bool processData(QnAbstractDataPacketPtr data);
	virtual void endOfRun();
	void cleanup();
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

};

#endif //_STREAM_RECORDER_H__
