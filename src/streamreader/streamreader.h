#ifndef stream_reader_514
#define stream_reader_514

#include "../base/longrunnable.h"
#include "../data/dataprocessor.h"
#include "../device/param.h"
#include <QMutex>
#include <QMutexLocker>
#include "../statistics/statistics.h"
#include <QHostAddress>
#include <QAuthenticator>
#include <QList>

typedef void* CLRestartHadlerInfo;
class CLStreamreader;
class CLDevice;

// onDeviceRestarted will be called if reader detects divec restart
struct CLDeviceRestartHadler
{
	virtual void onDeviceRestarted(CLStreamreader* reader, CLRestartHadlerInfo info) = 0;
};

#define CL_MAX_DATASIZE (20*1024*1024) // assume we can never get compressed data with  size greater than this
#define CL_MAX_CHANNEL_NUMBER (10) 


class CLStreamreader : public CLLongRunnable
{
public:
	enum StreamQuality {CLSLowest, CLSLow, CLSNormal, CLSHigh, CLSHighest};

	explicit CLStreamreader(CLDevice* dev);
	virtual ~CLStreamreader();

	
	void setStatistics(CLStatistics* stat);
	virtual void setStreamParams(CLParamList newParam);
	CLParamList getStreamParam() const;
	

	void addDataProcessor(CLAbstractDataProcessor* dp);
	void removeDataProcessor(CLAbstractDataProcessor* dp);


	virtual void setNeedKeyData();
	virtual bool needKeyData() const;

	void setDeviceRestartHadlerInfo(CLRestartHadlerInfo);
	void setDeviceRestartHadler(CLDeviceRestartHadler* restartHandler);

	virtual void setQuality(StreamQuality q);
	StreamQuality getQuality() const;

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


	CLRestartHadlerInfo m_restartInfo;
	CLDeviceRestartHadler* m_restartHandler;

	CLDevice* m_device; // reader reads data from this device.

	StreamQuality m_qulity;


};

#endif //stream_reader_514