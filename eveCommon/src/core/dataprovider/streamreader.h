#ifndef stream_reader_514
#define stream_reader_514

#include "utils/common/longrunnable.h"
#include "../resource/resource_consumer.h"



class QnAbstractStreamDataProvider;
class QnResource;

#define CL_MAX_DATASIZE (10*1024*1024) // assume we can never get compressed data with  size greater than this
#define CL_MAX_CHANNEL_NUMBER (10) 

struct AVCodecContext;

class QN_EXPORT QnAbstractStreamDataProvider : public CLLongRunnable, public QnResourceConsumer
{
    Q_OBJECT
public:

	//enum QnStreamQuality {CLSLowest, CLSLow, CLSNormal, CLSHigh, CLSHighest};

	explicit QnAbstractStreamDataProvider(QnResource* resource);
	virtual ~QnAbstractStreamDataProvider();

    virtual bool dataCanBeAccepted() const;

	//void setStatistics(QnStatistics* stat);
	virtual void setStreamParams(QnParamList newParam);
	QnParamList getStreamParam() const;

	void addDataProcessor(QnAbstractDataConsumer* dp);
	void removeDataProcessor(QnAbstractDataConsumer* dp);

    virtual void setReverseMode(bool value) {}
    virtual bool isReverseMode() const { return false;}

    bool isConnectedToTheResource() const;

	void pauseDataProcessors()
	{
		foreach(QnAbstractDataConsumer* dataProcessor, m_dataprocessors) {
			dataProcessor->pause();
		}
	}

	void resumeDataProcessors()
	{
		foreach(QnAbstractDataConsumer* dataProcessor, m_dataprocessors) {
			dataProcessor->resume();
		}
	}

    void setNeedSleep(bool sleep);

    void setSpeed(double value);

    void disconnectFromResource();
signals:
    void audioParamsChanged(AVCodecContext * codec);
    void videoParamsChanged(AVCodecContext * codec);
	void realTimeStreamHint(bool value);
    void slowSourceHint();
protected:
	void putData(QnAbstractDataPacketPtr data);
    void beforeDisconnectFromResource();
protected:

	QList<QnAbstractDataConsumer*> m_dataprocessors;
	mutable QMutex m_proc_CS;

	mutable QMutex m_params_CS;
	QnParamList m_streamParam;

	//QnStatistics* m_stat;

	//int m_gotKeyFrame[CL_MAX_CHANNEL_NUMBER];

	//QnStreamQuality m_qulity;

	//volatile bool m_needSleep;

};

#endif //stream_reader_514
