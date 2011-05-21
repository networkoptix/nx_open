#ifndef cl_iqeye_clien_pul1721
#define cl_iqeye_clien_pul1721

#include "streamreader/spush_streamreader.h"
#include "network/simple_http_client.h"



class CLIQEyeStreamreader: public CLServerPushStreamreader
{
private:
    enum {BLOCK_SIZE = 1460};
public:
	CLIQEyeStreamreader(CLDevice* dev);
	virtual ~CLIQEyeStreamreader();

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

#endif //cl_android_clien_pul1721
