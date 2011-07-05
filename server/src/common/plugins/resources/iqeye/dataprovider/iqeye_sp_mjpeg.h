#ifndef cl_iqeye_serverpush_jpeg_1846
#define cl_iqeye_serverpush_jpeg_1846

#include "dataprovider/spush_streamdataprovider.h"



class CLSimpleHTTPClient;



class CLIQEyeMJPEGtreamreader: public QnServerPushDataProvider
{
private:
    enum {BLOCK_SIZE = 1460};
public:
	CLIQEyeMJPEGtreamreader(QnResource* dev);
	virtual ~CLIQEyeMJPEGtreamreader();

protected:
    virtual QnAbstractMediaDataPacketPtr getNextData();
    virtual void openStream();
    virtual void closeStream() ;
    virtual bool isStreamOpened() const;
private:
    CLSimpleHTTPClient* mHttpClient;

    char mData[BLOCK_SIZE];
    int mDataRemainedBeginIndex;
    int mReaded;


};

#endif //cl_iqeye_serverpush_jpeg_1846
