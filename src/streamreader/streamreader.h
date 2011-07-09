#ifndef stream_reader_514
#define stream_reader_514

#include "../base/longrunnable.h"
#include "../data/dataprocessor.h"
#include "../device/param.h"
#include "../statistics/statistics.h"

class CLStreamreader;
class CLDevice;

#define CL_MAX_DATASIZE (10*1024*1024) // assume we can never get compressed data with  size greater than this
#define CL_MAX_CHANNEL_NUMBER (10) 

struct AVCodecContext;

class CLStreamreader : public CLLongRunnable
{
    Q_OBJECT
public:

	enum StreamQuality {CLSLowest, CLSLow, CLSNormal, CLSHigh, CLSHighest};

	explicit CLStreamreader(CLDevice* dev);
	virtual ~CLStreamreader();

	CLDevice* getDevice() const;

    virtual bool dataCanBeAccepted() const;

	void setStatistics(CLStatistics* stat);
	virtual void setStreamParams(CLParamList newParam);
	CLParamList getStreamParam() const;

	void addDataProcessor(CLAbstractDataProcessor* dp);
	void removeDataProcessor(CLAbstractDataProcessor* dp);

	void pauseDataProcessors()
	{
		foreach(CLAbstractDataProcessor* dataProcessor, m_dataprocessors) {
			dataProcessor->pause();
		}
	}

	void resumeDataProcessors()
	{
		foreach(CLAbstractDataProcessor* dataProcessor, m_dataprocessors) {
			dataProcessor->resume();
		}
	}

    void setNeedSleep(bool sleep);

	virtual void setNeedKeyData();
	virtual bool needKeyData(int channel) const;
	virtual bool needKeyData() const;

	virtual void setQuality(StreamQuality q);
	StreamQuality getQuality() const;
signals:
    void audioParamsChanged(AVCodecContext * codec);
    void videoParamsChanged(AVCodecContext * codec);
	void realTimeStreamHint(bool value);
protected:

	void putData(CLAbstractData* data);

protected:

	QList<CLAbstractDataProcessor*> m_dataprocessors;
	QMutex m_proc_CS;

	mutable QMutex m_params_CS;
	CLParamList m_streamParam;

	CLStatistics* m_stat;

	int m_gotKeyFrame[CL_MAX_CHANNEL_NUMBER];
	int m_channel_number;

	CLDevice* m_device; // reader reads data from this device.

	StreamQuality m_qulity;

	//volatile bool m_needSleep;

};

#endif //stream_reader_514
