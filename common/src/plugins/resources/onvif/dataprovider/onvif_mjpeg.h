#ifndef cl_mjpeg_dataprovider_h_1140
#define cl_mjpeg_dataprovider_h_1140

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/simple_http_client.h"
#include "core/dataprovider/live_stream_provider.h"




class MJPEGtreamreader: public CLServerPushStreamreader , public QnLiveStreamProvider
{
public:
	MJPEGtreamreader(QnResourcePtr res, const QString& requst);
	virtual ~MJPEGtreamreader();

protected:
    virtual QnAbstractMediaDataPtr getNextData();
    virtual void openStream();
    virtual void closeStream() ;
    virtual bool isStreamOpened() const;

    void updateStreamParamsBasedOnQuality() override {};
    void updateStreamParamsBasedOnFps() override {};

private:
    CLSimpleHTTPClient* mHttpClient;

    QString m_request;
};

#endif //cl_mjpeg_dataprovider_h_1140
