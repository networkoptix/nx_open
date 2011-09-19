#ifndef cl_mjpeg_dataprovider_h_1140
#define cl_mjpeg_dataprovider_h_1140

#include "streamreader/spush_streamreader.h"
#include "network/simple_http_client.h"



class MJPEGtreamreader: public CLServerPushStreamreader
{
private:
    enum {BLOCK_SIZE = 1460};
public:
	MJPEGtreamreader(CLDevice* dev, const QString& requst);
	virtual ~MJPEGtreamreader();

protected:
    virtual CLAbstractMediaData* getNextData();
    virtual void openStream();
    virtual void closeStream() ;
    virtual bool isStreamOpened() const;
private:
    CLSimpleHTTPClient* mHttpClient;

    char mData[BLOCK_SIZE];
    int mDataRemainedBeginIndex;
    int mReaded;

    QString m_request;


};

#endif //cl_mjpeg_dataprovider_h_1140
