#ifndef cl_mjpeg_dataprovider_h_1140
#define cl_mjpeg_dataprovider_h_1140

#include "core/dataprovider/spush_media_stream_provider.h"
#include "utils/network/simple_http_client.h"




class MJPEGtreamreader: public CLServerPushStreamreader
{
private:
    enum {BLOCK_SIZE = 1460 * 2};
public:
	MJPEGtreamreader(QnResourcePtr res, const QString& requst);
	virtual ~MJPEGtreamreader();

protected:
    virtual QnAbstractMediaDataPtr getNextData();
    virtual void openStream();
    virtual void closeStream() ;
    virtual bool isStreamOpened() const;
private:
    CLSimpleHTTPClient* mHttpClient;

    char mData[BLOCK_SIZE];
    int mDataRemainedBeginIndex;
    int mReaded;

    QString m_request;
    CLByteArray m_videoDataBuffer;
};

#endif //cl_mjpeg_dataprovider_h_1140
