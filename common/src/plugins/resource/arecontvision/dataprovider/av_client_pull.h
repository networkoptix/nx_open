#ifndef cl_av_clien_pull1529
#define cl_av_clien_pull1529

#ifdef ENABLE_ARECONT

#include "core/dataprovider/cpull_media_stream_provider.h"


struct AVLastPacketSize
{
    int x0, y0, width, height;

    AVLastPacketSize()
    :   x0(0), y0(0), width(0), height(0)
    {
    }
};

class QnPlAVClinetPullStreamReader : public QnClientPullMediaStreamProvider
{
    Q_OBJECT
public:
    QnPlAVClinetPullStreamReader(const QnResourcePtr& res);
    virtual ~QnPlAVClinetPullStreamReader();


protected:
    virtual void pleaseReopenStream(bool qualityChanged = true) override;

    int getBitrateMbps() const;
    bool isH264() const;
    void updateCameraParams();

private:
    QSize getMaxSensorSize(const QnResourcePtr& res) const;

private slots:
    void at_resourceInitDone(const QnResourcePtr &resource);

protected:
    // in av cameras you do not know the size of the frame in advance; 
    //so we can save a lot of memory by receiving all frames in this buff 
    // but will slow down a bit coz of extra memcpy ( I think not much )
    QnByteArray m_videoFrameBuff; 
    bool m_needUpdateParams;
    mutable QMutex m_needUpdateMtx;
};

#endif

#endif //cl_av_clien_pull1529

