#ifndef cl_iqeye_serverpush_jpeg_1846
#define cl_iqeye_serverpush_jpeg_1846

#include "streamreader/spush_streamreader.h"
#include "network/simple_http_client.h"



class CLIQEyeMJPEGtreamreader: public CLServerPushStreamreader
{
private:
    enum {BLOCK_SIZE = 1460};
public:
	CLIQEyeMJPEGtreamreader(CLDevice* dev);
	virtual ~CLIQEyeMJPEGtreamreader();

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


};

#endif //cl_iqeye_serverpush_jpeg_1846
