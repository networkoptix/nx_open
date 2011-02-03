#ifndef stream_reader_514
#define stream_reader_514

#include "../base/longrunnable.h"
#include "../data/dataprocessor.h"
#include "../device/param.h"
#include "../statistics/statistics.h"

class CLStreamreader;
class CLDevice;


#define CL_MAX_DATASIZE (20*1024*1024) // assume we can never get compressed data with  size greater than this
#define CL_MAX_CHANNEL_NUMBER (10) 


class CLStreamreader : public CLLongRunnable
{
public:
	enum StreamQuality {CLSLowest, CLSLow, CLSNormal, CLSHigh, CLSHighest};

	explicit CLStreamreader(CLDevice* dev);
	virtual ~CLStreamreader();

	CLDevice* getDevice() const;

	
	void setStatistics(CLStatistics* stat);
	virtual void setStreamParams(CLParamList newParam);
	CLParamList getStreamParam() const;
	

	void addDataProcessor(CLAbstractDataProcessor* dp);
	void removeDataProcessor(CLAbstractDataProcessor* dp);


	virtual void setNeedKeyData();
	virtual bool needKeyData(int channel) const;
	virtual bool needKeyData() const;

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


	CLDevice* m_device; // reader reads data from this device.

	StreamQuality m_qulity;


};

#endif //stream_reader_514