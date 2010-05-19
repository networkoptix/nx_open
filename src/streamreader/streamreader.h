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

class CLStreamreader : public CLLongRunnable
{
public:
	explicit CLStreamreader(CLDevice* dev);
	virtual ~CLStreamreader();

	
	void setStatistics(CLStatistics* stat);
	virtual void setStreamParams(CLParamList newParam);
	CLParamList getStreamParam() const;
	

	void addDataProcessor(CLAbstractDataProcessor* dp);
	void removeDataProcessor(CLAbstractDataProcessor* dp);


	void NeedKeyData();

	void setDeviceRestartHadlerInfo(CLRestartHadlerInfo);
	void setDeviceRestartHadler(CLDeviceRestartHadler* restartHandler);

protected:
	// returns number of channel related to last data arrived 
	virtual unsigned int getChannelNumber()= 0;

	void putData(CLAbstractData* data);

protected:



	QList<CLAbstractDataProcessor*> m_dataprocessors;
	QMutex m_proc_CS;


	mutable QMutex m_params_CS;
	CLParamList m_streamParam;
	


	CLStatistics* m_stat;

	volatile bool m_needKeyData;

	CLRestartHadlerInfo m_restartInfo;
	CLDeviceRestartHadler* m_restartHandler;

	CLDevice* m_device; // reader reads data from this device.

};

#endif //stream_reader_514