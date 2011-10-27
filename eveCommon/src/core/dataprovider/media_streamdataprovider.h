#ifndef QnMediaStreamDataProvider_514
#define QnMediaStreamDataProvider_514

#include "statistics/streamdata_statistics.h"
#include "streamreader/streamreader.h"
#include "device/qnresource.h"
#include "device/param.h"
#include "device/media_resource.h"

#define CL_MAX_DATASIZE (10*1024*1024) // assume we can never get compressed data with  size greater than this
#define CL_MAX_CHANNEL_NUMBER (4) 

class QnDeviceVideoLayout;
class QnDeviceAudioLayout;

class QnAbstractMediaStreamDataProvider : public QnAbstractStreamDataProvider
{
public:
    virtual QnDeviceVideoLayout* getVideoLayout();
    virtual QnDeviceAudioLayout* getAudioLayout();

	explicit QnAbstractMediaStreamDataProvider(QnResource* res);
	virtual ~QnAbstractMediaStreamDataProvider();


	const QnStatistics* getStatistics(int channel) const;

	virtual void setNeedKeyData();
	virtual bool needKeyData(int channel) const;
	virtual bool needKeyData() const;

	virtual void setQuality(QnStreamQuality q);
	QnStreamQuality getQuality() const;

protected:
    virtual void sleepIfNeeded() {}

    virtual void beforeRun();
    virtual void afterRun();

    virtual bool beforeGetData() { return true; }
    // if function returns false we do not put result into the queues
    virtual bool afterGetData(QnAbstractDataPacketPtr data);

    virtual void updateStreamParamsBasedOnQuality() {}

protected:
    int m_channel_number;


	QnStatistics m_stat[CL_MAX_CHANNEL_NUMBER];
	int m_gotKeyFrame[CL_MAX_CHANNEL_NUMBER];
	//int m_NumaberOfVideoChannels;
	QnStreamQuality m_qulity;
    QnParamList m_streamParam;

    int mFramesLost;
    QnMediaResource* m_mediaResource;
};


#endif //QnMediaStreamDataProvider_514
