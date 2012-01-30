#ifndef QnMediaStreamDataProvider_514
#define QnMediaStreamDataProvider_514

#include "core/dataprovider/statistics.h"
#include "../resource/media_resource.h"
#include "abstract_streamdataprovider.h"
#include "../datapacket/mediadatapacket.h"

class QnVideoResourceLayout;
class QnResourceAudioLayout;


#define MAX_LIVE_FPS 10000000.0
#define MAX_LOST_FRAME 2

class QnAbstractMediaStreamDataProvider : public QnAbstractStreamDataProvider
{
    Q_OBJECT;

public:
	explicit QnAbstractMediaStreamDataProvider(QnResourcePtr res);
	virtual ~QnAbstractMediaStreamDataProvider();


	const QnStatistics* getStatistics(int channel) const;

	virtual void setNeedKeyData();
	virtual bool needKeyData(int channel) const;
	virtual bool needKeyData() const;


protected:
    virtual QnAbstractMediaDataPtr getNextData() = 0;

    virtual void sleepIfNeeded() {}

    virtual void beforeRun();
    virtual void afterRun();

    virtual bool beforeGetData() { return true; }
    // if function returns false we do not put result into the queues
    virtual bool afterGetData(QnAbstractDataPacketPtr data);
    
    void checkTime(QnAbstractMediaDataPtr data);
protected:
    int m_channel_number;

	QnStatistics m_stat[CL_MAX_CHANNEL_NUMBER];
	int m_gotKeyFrame[CL_MAX_CHANNEL_NUMBER];

    int mFramesLost;
    QnMediaResourcePtr m_mediaResource;

private:
    mutable int m_numberOfchannels;
    qint64 m_lastVideoTime[CL_MAX_CHANNELS];
};


#endif //QnMediaStreamDataProvider_514
