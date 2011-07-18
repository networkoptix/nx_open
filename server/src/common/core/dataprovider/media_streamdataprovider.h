#ifndef QnMediaStreamDataProvider_514
#define QnMediaStreamDataProvider_514

#include "abstract_streamdataprovider.h"
#include "streamdata_statistics.h"


#define CL_MAX_DATASIZE (10*1024*1024) // assume we can never get compressed data with  size greater than this
#define CL_MAX_CHANNEL_NUMBER (4) 

class QnAbstractMediaStreamDataProvider : public QnAbstractStreamDataProvider
{
public:
	enum StreamQuality {CLSLowest, CLSLow, CLSNormal, CLSHigh, CLSHighest};

	explicit QnAbstractMediaStreamDataProvider(QnResourcePtr res);
	virtual ~QnAbstractMediaStreamDataProvider();


	QnStatistics* getStatistics(int channel) const;

	virtual void setNeedKeyData();
	virtual bool needKeyData(int channel) const;
	virtual bool needKeyData() const;

	virtual void setQuality(StreamQuality q);
	StreamQuality getQuality() const;

protected:
    void sleepIfNeeded() = 0;

    virtual void beforeRun();
    virtual void afterRun();

    virtual bool beforeGetData() = 0;
    // if function returns false we do not put result into the queues
    virtual bool afterGetData(QnAbstractDataPacketPtr data);

protected:

	QnStatistics m_stat[CL_MAX_CHANNEL_NUMBER];
	int m_gotKeyFrame[CL_MAX_CHANNEL_NUMBER];
	int m_NumaberOfVideoChannels;
	StreamQuality m_qulity;

    int mFramesLost;

};

#endif //QnMediaStreamDataProvider_514
