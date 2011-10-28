#ifndef QnMediaStreamDataProvider_514
#define QnMediaStreamDataProvider_514

#include "core/dataprovider/streamdata_statistics.h"
#include "../resource/media_resource.h"
#include "abstract_streamdataprovider.h"




class QnVideoResourceLayout;
class QnResourceAudioLayout;

class QnAbstractMediaStreamDataProvider : public QnAbstractStreamDataProvider
{
public:
    virtual QnVideoResourceLayout* getVideoLayout();
    virtual QnResourceAudioLayout* getAudioLayout();

	explicit QnAbstractMediaStreamDataProvider(QnResourcePtr res);
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
    QnMediaResourcePtr m_mediaResource;
};


#endif //QnMediaStreamDataProvider_514
