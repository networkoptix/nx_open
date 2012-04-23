#ifndef cl_av_clien_pull1529
#define cl_av_clien_pull1529

#include "core/dataprovider/cpull_media_stream_provider.h"
#include "core/dataprovider/live_stream_provider.h"



struct AVLastPacketSize
{
	int x0, y0, width, height;
};

class QnPlAVClinetPullStreamReader : public QnClientPullMediaStreamProvider, public QnLiveStreamProvider
{
public:
	QnPlAVClinetPullStreamReader(QnResourcePtr res);
	virtual ~QnPlAVClinetPullStreamReader();


protected:
    void updateStreamParamsBasedOnFps() override{};
	virtual void updateStreamParamsBasedOnQuality() override; 
    virtual void updateCameraMotion(const QnMotionRegion& region) override;


	int getBitrate() const;
	bool isH264() const;

protected:
    // in av cameras you do not know the size of the frame in advance; 
    //so we can save a lot of memory by receiving all frames in this buff 
    // but will slow down a bit coz of extra memcpy ( I think not much )
    CLByteArray m_videoFrameBuff; 

};

#endif //cl_av_clien_pull1529

