#ifndef QnMediaStreamDataProvider_514
#define QnMediaStreamDataProvider_514

#include "abstract_streamdataprovider.h"
#include "streamdata_statistics.h"


#define CL_MAX_DATASIZE (10*1024*1024) // assume we can never get compressed data with  size greater than this
#define CL_MAX_CHANNEL_NUMBER (4) 

enum QnStreamQuality {QnQualityLowest, QnQualityLow, QnQualityNormal, QnQualityHigh, QnQualityHighest, QnQualityPreSeted};

class QnAbstractMediaStreamDataProvider : public QnAbstractStreamDataProvider
{
public:

	explicit QnAbstractMediaStreamDataProvider(QnResourcePtr res);
	virtual ~QnAbstractMediaStreamDataProvider();


	QnStatistics* getStatistics(int channel) const;

	virtual void setNeedKeyData();
	virtual bool needKeyData(int channel) const;
	virtual bool needKeyData() const;

	virtual void setQuality(QnStreamQuality q);
	QnStreamQuality getQuality() const;

protected:
    void sleepIfNeeded() = 0;

    virtual void beforeRun();
    virtual void afterRun();

    virtual bool beforeGetData() = 0;
    // if function returns false we do not put result into the queues
    virtual bool afterGetData(QnAbstractDataPacketPtr data);

    virtual void updateStreamParamsBasedOnQuality() = 0; 

protected:

	QnStatistics m_stat[CL_MAX_CHANNEL_NUMBER];
	int m_gotKeyFrame[CL_MAX_CHANNEL_NUMBER];
	int m_NumaberOfVideoChannels;
	QnStreamQuality m_qulity;
    QnParamList m_streamParam;

    int mFramesLost;

};

#endif //QnMediaStreamDataProvider_514
