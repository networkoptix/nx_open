#ifndef QnMediaStreamDataProvider_514
#define QnMediaStreamDataProvider_514

#include "core/dataprovider/statistics.h"
#include "../resource/media_resource.h"
#include "abstract_streamdataprovider.h"
#include "../datapacket/mediadatapacket.h"

class QnVideoResourceLayout;
class QnResourceAudioLayout;

#define MAX_LIVE_FPS 10000000.0
#define META_DATA_DURATION_MS 300

class QnAbstractMediaStreamDataProvider : public QnAbstractStreamDataProvider
{
public:
	explicit QnAbstractMediaStreamDataProvider(QnResourcePtr res);
	virtual ~QnAbstractMediaStreamDataProvider();


	const QnStatistics* getStatistics(int channel) const;

	virtual void setNeedKeyData();
	virtual bool needKeyData(int channel) const;
	virtual bool needKeyData() const;

	virtual void setQuality(QnStreamQuality q);
	QnStreamQuality getQuality() const;

    // for live providers only 
    virtual void setFps(float f);
    float getFps() const;
    bool isMaxFps() const;


protected:
    virtual QnAbstractMediaDataPtr getNextData() = 0;

    virtual void sleepIfNeeded() {}

    virtual void beforeRun();
    virtual void afterRun();

    virtual bool beforeGetData() { return true; }
    // if function returns false we do not put result into the queues
    virtual bool afterGetData(QnAbstractDataPacketPtr data);

    virtual void updateStreamParamsBasedOnQuality(){};
    virtual void updateStreamParamsBasedOnFps(){};

    bool needMetaData() const; // used for live only 

protected:
    int m_channel_number;

	QnStatistics m_stat[CL_MAX_CHANNEL_NUMBER];
	int m_gotKeyFrame[CL_MAX_CHANNEL_NUMBER];
	//int m_NumaberOfVideoChannels;
	QnStreamQuality m_quality;

    float m_fps; //used only for live providers
    unsigned int m_framesSinceLastMetaData; // used only for live providers
    QTime m_timeSinceLastMetaData; //used only for live providers

    int mFramesLost;
    QnMediaResourcePtr m_mediaResource;

private:
    mutable int m_numberOfchannels;
};


#endif //QnMediaStreamDataProvider_514
