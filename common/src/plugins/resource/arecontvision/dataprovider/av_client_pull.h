#ifndef cl_av_clien_pull1529
#define cl_av_clien_pull1529

#ifdef ENABLE_ARECONT

#include "core/dataprovider/cpull_media_stream_provider.h"


struct AVLastPacketSize
{
    int x0, y0, width, height;
};

class QnPlAVClinetPullStreamReader : public QnClientPullMediaStreamProvider
{
    Q_OBJECT
public:
    QnPlAVClinetPullStreamReader(const QnResourcePtr& res);
    virtual ~QnPlAVClinetPullStreamReader();


protected:
    virtual void updateStreamParamsBasedOnFps() override {};
    virtual void updateStreamParamsBasedOnQuality() override; 
    //virtual void updateCameraMotion(const QnMotionRegion& region) override;


    int getBitrate() const;
    bool isH264() const;
    void updateCameraParams();
private slots:
    void at_resourceInitDone(const QnResourcePtr &resource);
protected:
    // in av cameras you do not know the size of the frame in advance; 
    //so we can save a lot of memory by receiving all frames in this buff 
    // but will slow down a bit coz of extra memcpy ( I think not much )
    QnByteArray m_videoFrameBuff; 
    bool m_needUpdateParams;
};

#endif

#endif //cl_av_clien_pull1529

