#ifndef stream_reader_514
#define stream_reader_514

#include "common/longrunnable.h"
#include "resource/resource_param.h"
#include "datapacket/datapacket.h"


class QnMediaStreamDataProvider;
class QnResource;
class QnStatistics;
class QnAbstractDataConsumer;


#define CL_MAX_DATASIZE (10*1024*1024) // assume we can never get compressed data with  size greater than this
#define CL_MAX_CHANNEL_NUMBER (10) 

class QnMediaStreamDataProvider : public QnLongRunnable
{
public:
	enum StreamQuality {CLSLowest, CLSLow, CLSNormal, CLSHigh, CLSHighest};

	explicit QnMediaStreamDataProvider(QnResource* dev);
	virtual ~QnMediaStreamDataProvider();

	QnResource* getDevice() const;

    virtual bool dataCanBeAccepted() const;

	void setStatistics(QnStatistics* stat);
	virtual void setStreamParams(QnParamList newParam);
	QnParamList getStreamParam() const;

	void addDataProcessor(QnAbstractDataConsumer* dp);
	void removeDataProcessor(QnAbstractDataConsumer* dp);

	void pauseDataProcessors();

	void resumeDataProcessors();

    void setNeedSleep(bool sleep);

	virtual void setNeedKeyData();
	virtual bool needKeyData(int channel) const;
	virtual bool needKeyData() const;

	virtual void setQuality(StreamQuality q);
	StreamQuality getQuality() const;

protected:

	void putData(QnAbstractDataPacketPtr data);

protected:

	QList<QnAbstractDataConsumer*> m_dataprocessors;
	QMutex m_proc_CS;

	mutable QMutex m_params_CS;
	QnParamList m_streamParam;

	QnStatistics* m_stat;

	int m_gotKeyFrame[CL_MAX_CHANNEL_NUMBER];
	int m_channel_number;

	QnResource* m_device; // reader reads data from this device.

	StreamQuality m_qulity;

	volatile bool m_needSleep;

};

#endif //stream_reader_514
